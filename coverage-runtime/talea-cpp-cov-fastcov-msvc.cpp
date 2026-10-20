// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 natyamatsya and talea-cpp-cov contributors

// TaleaCppCovApp "fast mode" runtime for MSVC (cl.exe).
//
// Link this into a target built with MSVC and
//   /fsanitize-coverage=inline-8bit-counters
// It works with the ordinary MSVC linker (no lld). Compile THIS file WITHOUT
// the coverage flag.
//
// MSVC places the 8-bit counters in the module's `.sancov` section and requires
// the runtime to define the `_Sancov8bitUsed` anchor. Unlike clang, there is no
// PC table and no init callback: the instrumented code increments counters
// directly with `inc byte [rip+disp]`, so a counter's address is only
// recoverable by disassembly, which the host importer does. This runtime just
// hands over the raw `.sancov` section and its module-relative address.
//
// The section is located by walking the module's own PE headers, rather than
// by planting section-boundary markers: a marker section can end up with
// different attributes than the compiler's counters (LNK4078) and thus not be
// merged with them, so the header walk is the reliable way to find the counters.
//
// Every module that carries counters links this runtime (instrumented DLLs need
// the `_Sancov8bitUsed` anchor), so each module has its own copy:
//   - The copy in the main executable dumps, at process exit, every loaded
//     module that has a `.sancov` section, so one file covers the process.
//   - The copy in a DLL dumps that DLL alone when the DLL is unloaded, so a
//     DLL freed before exit (FreeLibrary, QPluginLoader::unload) keeps its
//     coverage. It skips the dump when the executable's copy already wrote one
//     (DumpedEnvVar), which is the case for every DLL still loaded at exit.
//
// Paths have no length limit: the output path and the module paths are built
// on the heap, files are opened through `\\?\` paths, and the dump stores each
// module path with its length (FastCov::RawDumpVersion2).

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <windows.h>
#include <tlhelp32.h>

#include "FastCovFormat.hpp"

// The linker leaves this undefined in every instrumented object; defining it
// here lets the target link without the clang_rt SanitizerCoverage runtime.
extern "C" int _Sancov8bitUsed = 1;

// The base address of the module this copy of the runtime is linked into.
extern "C" IMAGE_DOS_HEADER __ImageBase;

namespace
{
	//-------------------------------------------------------------------------
	// A growable, null-terminated wide string on the process heap. The runtime
	// avoids the CRT allocator: it runs from exit handlers, some of them during
	// DLL unload.
	class WideBuffer
	{
	public:
		WideBuffer() = default;
		WideBuffer(const WideBuffer&) = delete;
		WideBuffer& operator=(const WideBuffer&) = delete;
		~WideBuffer()
		{
			if (data_)
				HeapFree(GetProcessHeap(), 0, data_);
		}

		// Capacity for chars characters plus the terminator.
		bool Reserve(DWORD chars)
		{
			if (chars < capacity_)
				return true;
			auto bytes = (static_cast<SIZE_T>(chars) + 1) * sizeof(wchar_t);
			auto* data = static_cast<wchar_t*>(
			    data_ ? HeapReAlloc(GetProcessHeap(), 0, data_, bytes)
			          : HeapAlloc(GetProcessHeap(), 0, bytes));
			if (!data)
				return false;
			data_ = data;
			capacity_ = chars + 1;
			return true;
		}

		bool Append(const wchar_t* text, DWORD chars)
		{
			if (!Reserve(length_ + chars))
				return false;
			std::memcpy(data_ + length_, text, chars * sizeof(wchar_t));
			length_ += chars;
			data_[length_] = L'\0';
			return true;
		}

		bool Append(wchar_t c) { return Append(&c, 1); }

		bool Append(const wchar_t* text)
		{
			return Append(text, static_cast<DWORD>(std::wcslen(text)));
		}

		// For APIs that fill the buffer themselves; length_ is set afterwards.
		wchar_t* Data() { return data_; }
		DWORD Capacity() const { return capacity_; }
		void SetLength(DWORD length)
		{
			length_ = length;
			data_[length_] = L'\0';
		}

		const wchar_t* Get() const { return data_; }
		DWORD Length() const { return length_; }
		bool Empty() const { return length_ == 0; }

	private:
		wchar_t* data_ = nullptr;
		DWORD capacity_ = 0;
		DWORD length_ = 0;
	};

	//-------------------------------------------------------------------------
	bool IsSeparator(wchar_t c) { return c == L'\\' || c == L'/'; }

	//-------------------------------------------------------------------------
	bool GetEnvironment(const wchar_t* name, WideBuffer& value)
	{
		auto required = GetEnvironmentVariableW(name, nullptr, 0);
		if (required == 0 || !value.Reserve(required))
			return false;
		auto length = GetEnvironmentVariableW(name, value.Data(), required);
		if (length == 0 || length >= required)
			return false;
		value.SetLength(length);
		return true;
	}

	//-------------------------------------------------------------------------
	// The full path of a loaded module, whatever its length.
	bool GetModulePath(HMODULE module, WideBuffer& path)
	{
		for (DWORD size = MAX_PATH; size <= 0x10000; size *= 2)
		{
			if (!path.Reserve(size))
				return false;
			auto length = GetModuleFileNameW(module, path.Data(), size);
			if (length == 0)
				return false;
			if (length < size)
			{
				path.SetLength(length);
				return true;
			}
		}
		return false;
	}

	//-------------------------------------------------------------------------
	// path as a `\\?\` path, which the file APIs accept beyond MAX_PATH. An
	// absolute path gets the prefix directly (separators normalized, runs of
	// separators collapsed: the prefix disables that normalization); a relative
	// one is made absolute first.
	bool ToExtendedPath(const WideBuffer& path, WideBuffer& extended)
	{
		const auto* p = path.Get();
		if (std::wcsncmp(p, L"\\\\?\\", 4) == 0)
			return extended.Append(p, path.Length());

		WideBuffer absolute;
		bool unc = IsSeparator(p[0]) && IsSeparator(p[1]);
		bool drive = p[0] != L'\0' && p[1] == L':' && IsSeparator(p[2]);
		if (unc || drive)
		{
			if (!absolute.Append(p, path.Length()))
				return false;
		}
		else
		{
			auto required = GetFullPathNameW(p, 0, nullptr, nullptr);
			if (required == 0 || !absolute.Reserve(required))
				return false;
			auto length =
			    GetFullPathNameW(p, required, absolute.Data(), nullptr);
			if (length == 0 || length >= required)
				return false;
			absolute.SetLength(length);
			unc = IsSeparator(absolute.Get()[0]) &&
			      IsSeparator(absolute.Get()[1]);
		}

		const auto* a = absolute.Get();
		DWORD i = 0;
		if (unc)
		{
			if (!extended.Append(L"\\\\?\\UNC\\"))
				return false;
			i = 2;
		}
		else if (!extended.Append(L"\\\\?\\"))
			return false;

		bool previousSeparator = false;
		for (; i < absolute.Length(); ++i)
		{
			bool separator = IsSeparator(a[i]);
			if (separator && previousSeparator)
				continue;
			if (!extended.Append(separator ? L'\\' : a[i]))
				return false;
			previousSeparator = separator;
		}
		return true;
	}

	//-------------------------------------------------------------------------
	// The module's file name without directory and extension.
	void AppendStem(WideBuffer& out, HMODULE module)
	{
		WideBuffer path;
		if (!GetModulePath(module, path))
			return;
		DWORD start = 0;
		DWORD end = path.Length();
		for (DWORD i = 0; i < path.Length(); ++i)
			if (IsSeparator(path.Get()[i]))
				start = i + 1;
		for (DWORD i = end; i > start; --i)
			if (path.Get()[i - 1] == L'.')
			{
				end = i - 1;
				break;
			}
		out.Append(path.Get() + start, end - start);
	}

	//-------------------------------------------------------------------------
	void AppendNumber(WideBuffer& out, std::uint64_t value)
	{
		wchar_t digits[24];
		DWORD count = 0;
		do
		{
			digits[count++] = static_cast<wchar_t>(L'0' + (value % 10));
			value /= 10;
		} while (value != 0 && count < 24);
		while (count != 0)
			out.Append(digits[--count]);
	}

	//-------------------------------------------------------------------------
	// Where to write the dump, as a `\\?\` path. When the output path names a
	// directory (or ends with a separator), the dump goes to a per-process
	// file inside it - "<exe-stem>_<pid>.fastcov", and for a DLL dumped at
	// unload "<exe-stem>_<pid>_<dll-stem>_<tick>.fastcov" - so a parallel
	// ctest -jN suite produces one dump per test process instead of every
	// process clobbering a single shared file. A plain file path is used
	// verbatim (an exact target, as in the importer round-trip test).
	bool GetDumpPath(HMODULE dumpedDll, WideBuffer& dumpPath)
	{
		WideBuffer output;
		if (!GetEnvironment(FastCov::OutputPathEnvVar, output))
			return false;
		WideBuffer extended;
		if (!ToExtendedPath(output, extended))
			return false;

		auto attrs = GetFileAttributesW(extended.Get());
		bool isDir = IsSeparator(output.Get()[output.Length() - 1]) ||
		             (attrs != INVALID_FILE_ATTRIBUTES &&
		              (attrs & FILE_ATTRIBUTE_DIRECTORY) != 0);
		if (!dumpPath.Append(extended.Get(), extended.Length()))
			return false;
		if (!isDir)
			return true;

		if (!IsSeparator(extended.Get()[extended.Length() - 1]))
			dumpPath.Append(L'\\');
		AppendStem(dumpPath, nullptr);
		dumpPath.Append(L'_');
		AppendNumber(dumpPath, GetCurrentProcessId());
		if (dumpedDll)
		{
			// A DLL can be loaded and unloaded repeatedly within one process;
			// the counter value keeps each unload's dump distinct.
			LARGE_INTEGER tick{};
			QueryPerformanceCounter(&tick);
			dumpPath.Append(L'_');
			AppendStem(dumpPath, dumpedDll);
			dumpPath.Append(L'_');
			AppendNumber(dumpPath, static_cast<std::uint64_t>(tick.QuadPart));
		}
		return dumpPath.Append(L".fastcov");
	}

	//-------------------------------------------------------------------------
	void WriteAll(HANDLE file, const void* data, DWORD size)
	{
		DWORD written = 0;
		WriteFile(file, data, size, &written, nullptr);
	}

	//-------------------------------------------------------------------------
	// The module-relative address and size of the ".sancov" section, or {0,0}.
	bool FindSancovSection(std::uint8_t* base,
	                       std::uint32_t& rva,
	                       std::uint32_t& size)
	{
		auto* dosHeader = reinterpret_cast<IMAGE_DOS_HEADER*>(base);
		if (dosHeader->e_magic != IMAGE_DOS_SIGNATURE)
			return false;
		auto* ntHeaders = reinterpret_cast<IMAGE_NT_HEADERS*>(
		    base + dosHeader->e_lfanew);
		if (ntHeaders->Signature != IMAGE_NT_SIGNATURE)
			return false;

		auto* section = IMAGE_FIRST_SECTION(ntHeaders);
		for (WORD i = 0; i < ntHeaders->FileHeader.NumberOfSections;
		     ++i, ++section)
		{
			if (std::memcmp(section->Name, ".sancov", 8) == 0)
			{
				rva = section->VirtualAddress;
				size = section->Misc.VirtualSize;
				return size != 0;
			}
		}
		return false;
	}

	//-------------------------------------------------------------------------
	// Writes one module record: RawModuleHeaderV2, the path, then the region.
	void WriteModule(HANDLE file,
	                 HMODULE module,
	                 std::uint32_t regionRva,
	                 std::uint32_t regionSize)
	{
		WideBuffer path;
		GetModulePath(module, path);

		FastCov::RawModuleHeaderV2 moduleHeader{};
		moduleHeader.pathChars_ = path.Length();
		moduleHeader.regionRva_ = regionRva;
		moduleHeader.regionSize_ = regionSize;

		WriteAll(file, &moduleHeader, sizeof(moduleHeader));
		WriteAll(file,
		         path.Get(),
		         static_cast<DWORD>(path.Length() * sizeof(wchar_t)));
		WriteAll(file,
		         reinterpret_cast<std::uint8_t*>(module) + regionRva,
		         regionSize);
	}

	//-------------------------------------------------------------------------
	HANDLE CreateDump(HMODULE dumpedDll, std::uint32_t moduleCount)
	{
		WideBuffer dumpPath;
		if (!GetDumpPath(dumpedDll, dumpPath))
			return INVALID_HANDLE_VALUE;
		auto file = CreateFileW(dumpPath.Get(),
		                        GENERIC_WRITE,
		                        0,
		                        nullptr,
		                        CREATE_ALWAYS,
		                        FILE_ATTRIBUTE_NORMAL,
		                        nullptr);
		if (file == INVALID_HANDLE_VALUE)
			return file;

		FastCov::RawDumpHeader header{};
		header.magic_ = FastCov::RawDumpMagic;
		header.version_ = FastCov::RawDumpVersion2;
		header.moduleCount_ = moduleCount;
		header.reserved_ = 0;
		WriteAll(file, &header, sizeof(header));
		return file;
	}

	//-------------------------------------------------------------------------
	// Process exit, in the main executable's copy: dump every loaded module
	// with a .sancov section - the executable and any instrumented DLL. System
	// and uninstrumented modules have no such section and are skipped.
	void OnExit()
	{
		// Tells the DLL copies, whose unload handlers run after this one, that
		// their modules are in this dump.
		SetEnvironmentVariableW(FastCov::DumpedEnvVar, L"1");

		auto snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, 0);
		if (snapshot == INVALID_HANDLE_VALUE)
			return;

		MODULEENTRY32W entry{};
		entry.dwSize = sizeof(entry);

		// First pass: count instrumented modules so moduleCount_ is known
		// before any module data is written (and no file is created for none).
		std::uint32_t moduleCount = 0;
		for (BOOL more = Module32FirstW(snapshot, &entry); more;
		     more = Module32NextW(snapshot, &entry))
		{
			std::uint32_t rva = 0;
			std::uint32_t size = 0;
			if (FindSancovSection(entry.modBaseAddr, rva, size))
				++moduleCount;
		}
		if (moduleCount == 0)
		{
			CloseHandle(snapshot);
			return;
		}

		auto file = CreateDump(nullptr, moduleCount);
		if (file == INVALID_HANDLE_VALUE)
		{
			CloseHandle(snapshot);
			return;
		}

		// Second pass: write each instrumented module. The snapshot order is
		// stable across the two passes.
		entry.dwSize = sizeof(entry);
		for (BOOL more = Module32FirstW(snapshot, &entry); more;
		     more = Module32NextW(snapshot, &entry))
		{
			std::uint32_t regionRva = 0;
			std::uint32_t regionSize = 0;
			if (FindSancovSection(entry.modBaseAddr, regionRva, regionSize))
				WriteModule(file, entry.hModule, regionRva, regionSize);
		}

		CloseHandle(file);
		CloseHandle(snapshot);
	}

	//-------------------------------------------------------------------------
	// DLL unload, in a DLL's copy: dump this DLL alone, unless the executable's
	// copy already wrote it at process exit. Runs during DLL_PROCESS_DETACH, so
	// it only calls kernel32 file and memory functions.
	void OnUnload()
	{
		WideBuffer dumped;
		if (GetEnvironment(FastCov::DumpedEnvVar, dumped))
			return;

		auto module = reinterpret_cast<HMODULE>(&__ImageBase);
		std::uint32_t regionRva = 0;
		std::uint32_t regionSize = 0;
		if (!FindSancovSection(reinterpret_cast<std::uint8_t*>(module),
		                       regionRva,
		                       regionSize))
			return;

		auto file = CreateDump(module, 1);
		if (file == INVALID_HANDLE_VALUE)
			return;
		WriteModule(file, module, regionRva, regionSize);
		CloseHandle(file);
	}

	//-------------------------------------------------------------------------
	// Registers the dump handler. A namespace-scope object's constructor runs
	// during CRT startup; that is fine here (unlike the counter storage, this
	// object is not touched by the compiler's coverage instrumentation). In a
	// DLL, atexit registers with the DLL's own CRT, which runs the handler when
	// the DLL is unloaded.
	struct Registrar
	{
		Registrar()
		{
			auto self = reinterpret_cast<HMODULE>(&__ImageBase);
			atexit(self == GetModuleHandleW(nullptr) ? OnExit : OnUnload);
		}
	} g_registrar;
}
