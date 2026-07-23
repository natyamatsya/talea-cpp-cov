// OpenCppCoverage is an open source code coverage for C++.
// Copyright (C) 2026 OpenCppCoverage
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

// OpenCppCoverage "fast mode" runtime for MSVC (cl.exe).
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
// The section is located by walking the module's own PE headers at exit, rather
// than by planting section-boundary markers: a marker section can end up with
// different attributes than the compiler's counters (LNK4078) and thus not be
// merged with them, so the header walk is the reliable way to find the counters.
//
// At exit the runtime enumerates every loaded module and dumps each one that
// carries a `.sancov` section - the main executable plus any instrumented DLL -
// so coverage spans the whole process, not just the exe. Instrumented DLLs must
// link this runtime too (they need the `_Sancov8bitUsed` anchor); to keep a
// single dump, only the copy living in the main executable registers the exit
// handler (see IsMainExecutable).

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <windows.h>
#include <tlhelp32.h>

#include "FastCovFormat.hpp"

// The linker leaves this undefined in every instrumented object; defining it
// here lets the target link without the clang_rt SanitizerCoverage runtime.
extern "C" int _Sancov8bitUsed = 1;

namespace
{
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
	void OnExit()
	{
		wchar_t path[FastCov::MaxModulePathChars];
		auto length = GetEnvironmentVariableW(
		    FastCov::OutputPathEnvVar, path, FastCov::MaxModulePathChars);
		if (length == 0 || length >= FastCov::MaxModulePathChars)
			return;

		// If the output path names a directory (or ends with a separator), write
		// a per-process file inside it, "<exe-stem>_<pid>.fastcov". A parallel
		// ctest -jN suite then produces one dump per test process instead of
		// every process clobbering a single shared file. A plain file path is
		// used verbatim (so an exact target - e.g. the importer round-trip test
		// - keeps its behavior).
		{
			auto attrs = GetFileAttributesW(path);
			bool isDir =
			    (length > 0 &&
			     (path[length - 1] == L'\\' || path[length - 1] == L'/')) ||
			    (attrs != INVALID_FILE_ATTRIBUTES &&
			     (attrs & FILE_ATTRIBUTE_DIRECTORY) != 0);
			if (isDir)
			{
				wchar_t exePath[FastCov::MaxModulePathChars];
				auto exeLen = GetModuleFileNameW(
				    nullptr, exePath, FastCov::MaxModulePathChars);
				std::uint32_t stemStart = 0;
				std::uint32_t stemEnd = exeLen;
				for (std::uint32_t i = 0; i < exeLen; ++i)
					if (exePath[i] == L'\\' || exePath[i] == L'/')
						stemStart = i + 1;
				for (std::uint32_t i = exeLen; i > stemStart; --i)
					if (exePath[i - 1] == L'.')
					{
						stemEnd = i - 1;
						break;
					}

				std::uint32_t p = length;
				auto put = [&](wchar_t c) {
					if (p + 1 < FastCov::MaxModulePathChars)
						path[p++] = c;
				};
				if (path[length - 1] != L'\\' && path[length - 1] != L'/')
					put(L'\\');
				for (std::uint32_t i = stemStart; i < stemEnd; ++i)
					put(exePath[i]);
				put(L'_');
				wchar_t pid[16];
				std::uint32_t pidLen = 0;
				auto id = GetCurrentProcessId();
				do
				{
					pid[pidLen++] = static_cast<wchar_t>(L'0' + (id % 10));
					id /= 10;
				} while (id != 0 && pidLen < 16);
				while (pidLen != 0)
					put(pid[--pidLen]);
				const wchar_t ext[] = L".fastcov";
				for (std::uint32_t i = 0; ext[i] != L'\0'; ++i)
					put(ext[i]);
				path[p] = L'\0';
			}
		}

		// Every loaded module with a .sancov section is instrumented: the main
		// executable and any instrumented DLL. System and uninstrumented
		// modules have no such section and are skipped.
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

		auto file = CreateFileW(path,
		                        GENERIC_WRITE,
		                        0,
		                        nullptr,
		                        CREATE_ALWAYS,
		                        FILE_ATTRIBUTE_NORMAL,
		                        nullptr);
		if (file == INVALID_HANDLE_VALUE)
		{
			CloseHandle(snapshot);
			return;
		}

		FastCov::RawDumpHeader header{};
		header.magic_ = FastCov::RawDumpMagic;
		header.version_ = FastCov::DumpVersion;
		header.moduleCount_ = moduleCount;
		header.reserved_ = 0;
		WriteAll(file, &header, sizeof(header));

		// Second pass: write each instrumented module's header and .sancov
		// region. The snapshot order is stable across the two passes.
		entry.dwSize = sizeof(entry);
		for (BOOL more = Module32FirstW(snapshot, &entry); more;
		     more = Module32NextW(snapshot, &entry))
		{
			std::uint32_t regionRva = 0;
			std::uint32_t regionSize = 0;
			if (!FindSancovSection(entry.modBaseAddr, regionRva, regionSize))
				continue;

			FastCov::RawModuleHeader moduleHeader{};
			GetModuleFileNameW(
			    entry.hModule, moduleHeader.path_, FastCov::MaxModulePathChars);
			moduleHeader.regionRva_ = regionRva;
			moduleHeader.regionSize_ = regionSize;

			WriteAll(file, &moduleHeader, sizeof(moduleHeader));
			WriteAll(file, entry.modBaseAddr + regionRva, regionSize);
		}

		CloseHandle(file);
		CloseHandle(snapshot);
	}

	//-------------------------------------------------------------------------
	// True if this runtime copy lives in the main executable. Instrumented DLLs
	// link this runtime for the _Sancov8bitUsed anchor and would each register
	// their own exit dump; gating on the main module keeps exactly one dump,
	// written by the executable's copy, which enumerates every module anyway.
	bool IsMainExecutable()
	{
		HMODULE self = nullptr;
		if (!GetModuleHandleExW(
		        GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
		            GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
		        reinterpret_cast<LPCWSTR>(&OnExit),
		        &self))
			return true; // best effort: dump rather than lose coverage
		return self == GetModuleHandleW(nullptr);
	}

	//-------------------------------------------------------------------------
	// Registers the exit dump. A namespace-scope object's constructor runs
	// during CRT startup; that is fine here (unlike the counter storage, this
	// object is not touched by the compiler's coverage instrumentation).
	struct Registrar
	{
		Registrar()
		{
			if (IsMainExecutable())
				atexit(OnExit);
		}
	} g_registrar;
}
