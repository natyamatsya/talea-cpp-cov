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

// OpenCppCoverage "fast mode" runtime.
//
// Link this into a target built with:
//   clang-cl -fsanitize-coverage=inline-8bit-counters,pc-table
//            -fuse-ld=lld   (lld is required: the MSVC linker does not resolve
//                            the sancov section-boundary symbols correctly)
// Compile THIS file WITHOUT -fsanitize-coverage, so the runtime does not
// instrument itself.
//
// The SanitizerCoverage callbacks fire during CRT initialization, before main,
// so all storage the callbacks touch is plain zero-initialized data (no C++
// constructors). On process exit the runtime writes every basic block's
// module-relative address, execution count, and function-entry flag to the file
// named by the OPENCPPCOVERAGE_FASTCOV_OUTPUT environment variable; the
// OpenCppCoverage host maps them back to source lines and template
// instantiations through the PDB.
//
// Deliberately freestanding: Win32 + plain data only, no STL globals, so it
// links cleanly into any clang-cl target regardless of CRT flavor.

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <windows.h>

#include "FastCovFormat.hpp"

namespace
{
	constexpr int MaxSections = 64;
	constexpr int MaxModules = 256;

	const std::uint8_t* g_counterBegin[MaxSections];
	const std::uint8_t* g_counterEnd[MaxSections];
	const std::uintptr_t* g_pcBegin[MaxSections];
	const std::uintptr_t* g_pcEnd[MaxSections];
	int g_counterSections;
	int g_pcSections;
	int g_atexitRegistered;

	//-------------------------------------------------------------------------
	std::uintptr_t ModuleBaseOf(std::uintptr_t address)
	{
		HMODULE module = nullptr;
		if (!GetModuleHandleExW(
		        GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
		            GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
		        reinterpret_cast<LPCWSTR>(address),
		        &module))
			return 0;
		return reinterpret_cast<std::uintptr_t>(module);
	}

	//-------------------------------------------------------------------------
	void WriteDump(const wchar_t* path)
	{
		// Pair each counter section with its PC section by registration order.
		int sections = g_counterSections < g_pcSections ? g_counterSections
		                                                : g_pcSections;

		// Pass 1: build the module table (base -> path, index) and count the
		// basic blocks. Fixed-size, no allocation.
		std::uintptr_t moduleBases[MaxModules];
		FastCov::ModuleEntry moduleEntries[MaxModules];
		int moduleCount = 0;
		std::uint32_t basicBlockCount = 0;

		auto moduleIndexOf = [&](std::uintptr_t base) -> int {
			for (int m = 0; m < moduleCount; ++m)
				if (moduleBases[m] == base)
					return m;
			if (moduleCount >= MaxModules)
				return -1;
			moduleBases[moduleCount] = base;
			auto& entry = moduleEntries[moduleCount];
			for (auto& c : entry.path_)
				c = L'\0';
			GetModuleFileNameW(reinterpret_cast<HMODULE>(base),
			                   entry.path_,
			                   FastCov::MaxModulePathChars);
			return moduleCount++;
		};

		for (int s = 0; s < sections; ++s)
		{
			std::size_t counters = g_counterEnd[s] - g_counterBegin[s];
			std::size_t pcPairs = (g_pcEnd[s] - g_pcBegin[s]) / 2;
			std::size_t n = counters < pcPairs ? counters : pcPairs;
			for (std::size_t i = 0; i < n; ++i)
			{
				auto base = ModuleBaseOf(g_pcBegin[s][2 * i]);
				if (base && moduleIndexOf(base) >= 0)
					++basicBlockCount;
			}
		}

		std::size_t totalSize =
		    sizeof(FastCov::DumpHeader) +
		    static_cast<std::size_t>(moduleCount) *
		        sizeof(FastCov::ModuleEntry) +
		    static_cast<std::size_t>(basicBlockCount) *
		        sizeof(FastCov::BasicBlockRecord);

		auto* buffer = static_cast<std::uint8_t*>(
		    HeapAlloc(GetProcessHeap(), 0, totalSize));
		if (!buffer)
			return;

		auto* header = reinterpret_cast<FastCov::DumpHeader*>(buffer);
		header->magic_ = FastCov::DumpMagic;
		header->version_ = FastCov::DumpVersion;
		header->moduleCount_ = static_cast<std::uint32_t>(moduleCount);
		header->basicBlockCount_ = basicBlockCount;

		auto* entries = reinterpret_cast<FastCov::ModuleEntry*>(
		    buffer + sizeof(FastCov::DumpHeader));
		for (int m = 0; m < moduleCount; ++m)
			entries[m] = moduleEntries[m];

		auto* records = reinterpret_cast<FastCov::BasicBlockRecord*>(
		    buffer + sizeof(FastCov::DumpHeader) +
		    static_cast<std::size_t>(moduleCount) *
		        sizeof(FastCov::ModuleEntry));

		// Pass 2: fill the basic-block records.
		std::uint32_t written = 0;
		for (int s = 0; s < sections && written < basicBlockCount; ++s)
		{
			std::size_t counters = g_counterEnd[s] - g_counterBegin[s];
			std::size_t pcPairs = (g_pcEnd[s] - g_pcBegin[s]) / 2;
			std::size_t n = counters < pcPairs ? counters : pcPairs;
			for (std::size_t i = 0; i < n && written < basicBlockCount; ++i)
			{
				auto pc = g_pcBegin[s][2 * i];
				auto base = ModuleBaseOf(pc);
				if (!base)
					continue;
				int moduleIndex = moduleIndexOf(base);
				if (moduleIndex < 0)
					continue;

				auto& record = records[written++];
				record.moduleIndex_ =
				    static_cast<std::uint32_t>(moduleIndex);
				record.rva_ = static_cast<std::uint32_t>(pc - base);
				record.count_ = g_counterBegin[s][i];
				record.isEntry_ = static_cast<std::uint8_t>(
				    g_pcBegin[s][2 * i + 1] & 1);
				record.reserved_ = 0;
			}
		}

		auto file = CreateFileW(path,
		                        GENERIC_WRITE,
		                        0,
		                        nullptr,
		                        CREATE_ALWAYS,
		                        FILE_ATTRIBUTE_NORMAL,
		                        nullptr);
		if (file != INVALID_HANDLE_VALUE)
		{
			DWORD bytesWritten = 0;
			WriteFile(file,
			          buffer,
			          static_cast<DWORD>(totalSize),
			          &bytesWritten,
			          nullptr);
			CloseHandle(file);
		}

		HeapFree(GetProcessHeap(), 0, buffer);
	}

	//-------------------------------------------------------------------------
	void OnExit()
	{
		wchar_t path[FastCov::MaxModulePathChars];
		auto length = GetEnvironmentVariableW(
		    FastCov::OutputPathEnvVar, path, FastCov::MaxModulePathChars);
		if (length != 0 && length < FastCov::MaxModulePathChars)
			WriteDump(path);
	}
}

//-----------------------------------------------------------------------------
extern "C" void __sanitizer_cov_8bit_counters_init(std::uint8_t* start,
                                                   std::uint8_t* stop)
{
	if (g_counterSections < MaxSections)
	{
		g_counterBegin[g_counterSections] = start;
		g_counterEnd[g_counterSections] = stop;
		++g_counterSections;
	}
	if (!g_atexitRegistered)
	{
		g_atexitRegistered = 1;
		atexit(OnExit);
	}
}

//-----------------------------------------------------------------------------
extern "C" void __sanitizer_cov_pcs_init(const std::uintptr_t* beg,
                                         const std::uintptr_t* end)
{
	if (g_pcSections < MaxSections)
	{
		g_pcBegin[g_pcSections] = beg;
		g_pcEnd[g_pcSections] = end;
		++g_pcSections;
	}
}
