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

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <windows.h>

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

		// v1 instruments the main executable, whose .sancov section holds the
		// counters.
		auto module = GetModuleHandleW(nullptr);
		auto base = reinterpret_cast<std::uint8_t*>(module);

		std::uint32_t regionRva = 0;
		std::uint32_t regionSize = 0;
		if (!FindSancovSection(base, regionRva, regionSize))
			return;
		auto* regionStart = base + regionRva;

		FastCov::RawModuleHeader moduleHeader{};
		GetModuleFileNameW(
		    module, moduleHeader.path_, FastCov::MaxModulePathChars);
		moduleHeader.regionRva_ = regionRva;
		moduleHeader.regionSize_ = regionSize;

		FastCov::RawDumpHeader header{};
		header.magic_ = FastCov::RawDumpMagic;
		header.version_ = FastCov::DumpVersion;
		header.moduleCount_ = 1;
		header.reserved_ = 0;

		auto file = CreateFileW(path,
		                        GENERIC_WRITE,
		                        0,
		                        nullptr,
		                        CREATE_ALWAYS,
		                        FILE_ATTRIBUTE_NORMAL,
		                        nullptr);
		if (file == INVALID_HANDLE_VALUE)
			return;

		WriteAll(file, &header, sizeof(header));
		WriteAll(file, &moduleHeader, sizeof(moduleHeader));
		WriteAll(file, regionStart, regionSize);
		CloseHandle(file);
	}

	//-------------------------------------------------------------------------
	// Registers the exit dump. A namespace-scope object's constructor runs
	// during CRT startup; that is fine here (unlike the counter storage, this
	// object is not touched by the compiler's coverage instrumentation).
	struct Registrar
	{
		Registrar() { atexit(OnExit); }
	} g_registrar;
}
