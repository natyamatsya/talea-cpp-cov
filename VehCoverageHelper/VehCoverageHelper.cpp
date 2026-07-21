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

// In-process coverage helper.
//
// The host injects this DLL into the target (launched suspended) and creates
// a shared section naming the probe RVAs. On attach - while the target's main
// thread is still suspended - the helper plants an int3 at each probe and
// installs a Vectored Exception Handler. When a probe first executes, the VEH
// records the hit in the shared bitmap and restores the original byte, all in
// process: a usermode exception dispatch (microseconds) instead of the
// cross-process debug-event round trip (~100 microseconds) the debugger engine
// pays per hit.
//
// Deliberately tiny and dependency-free (kernel32 only, no CRT startup, no
// PCH) so it is safe to run from DllMain under the loader lock.

#include <Windows.h>

#include "VehProtocol.hpp"

namespace
{
	// Resolved once at attach; read (never written) by the exception handler.
	std::uint8_t* g_moduleBase = nullptr;
	const std::uint32_t* g_rvas = nullptr;
	std::uint8_t* g_hits = nullptr;
	std::uint8_t* g_originalBytes = nullptr;
	std::uint32_t g_probeCount = 0;
	std::uint32_t g_maxRva = 0;
	PVOID g_vehHandle = nullptr;
	HANDLE g_mappingHandle = nullptr;
	VehCoverage::SharedHeader* g_header = nullptr;

	//-------------------------------------------------------------------------
	// Index of rva in the sorted g_rvas array, or -1. Branchless-ish binary
	// search; runs on every int3, so it stays allocation- and lock-free.
	std::int32_t FindProbe(std::uint32_t rva) noexcept
	{
		std::uint32_t low = 0;
		std::uint32_t high = g_probeCount;
		while (low < high)
		{
			auto mid = low + (high - low) / 2;
			auto value = g_rvas[mid];
			if (value < rva)
				low = mid + 1;
			else if (value > rva)
				high = mid;
			else
				return static_cast<std::int32_t>(mid);
		}
		return -1;
	}

	//-------------------------------------------------------------------------
	LONG CALLBACK OnVectoredException(EXCEPTION_POINTERS* exceptionInfo) noexcept
	{
		auto* record = exceptionInfo->ExceptionRecord;
		if (record->ExceptionCode != EXCEPTION_BREAKPOINT)
			return EXCEPTION_CONTINUE_SEARCH;

		auto* address =
		    reinterpret_cast<std::uint8_t*>(record->ExceptionAddress);
		if (address < g_moduleBase || address > g_moduleBase + g_maxRva)
			return EXCEPTION_CONTINUE_SEARCH;

		auto rva = static_cast<std::uint32_t>(address - g_moduleBase);
		auto index = FindProbe(rva);
		if (index < 0)
			return EXCEPTION_CONTINUE_SEARCH; // the app's own int3

		// Record the hit and retire the probe: restore the original byte and
		// rewind the instruction pointer so the real instruction re-executes.
		// Concurrent hits on the same probe write the same byte and bit, so no
		// synchronization is needed (best-effort, like the debugger engine).
		g_hits[index] = 1;
		*address = g_originalBytes[index];
		FlushInstructionCache(GetCurrentProcess(), address, 1);

#ifdef _WIN64
		exceptionInfo->ContextRecord->Rip =
		    reinterpret_cast<DWORD64>(address);
#else
		exceptionInfo->ContextRecord->Eip =
		    reinterpret_cast<DWORD>(address);
#endif
		return EXCEPTION_CONTINUE_EXECUTION;
	}

	//-------------------------------------------------------------------------
	void Fail(VehCoverage::SharedHeader* header) noexcept
	{
		if (header)
		{
			header->helperErrorCode = GetLastError();
			header->status =
			    static_cast<std::uint32_t>(VehCoverage::HelperStatus::Error);
		}
	}

	//-------------------------------------------------------------------------
	bool Initialize() noexcept
	{
		wchar_t mappingName[64];
		wchar_t* cursor = mappingName;
		for (const wchar_t* p = VehCoverage::MappingNamePrefix; *p; ++p)
			*cursor++ = *p;

		// Append the decimal process id without the CRT.
		auto pid = GetCurrentProcessId();
		wchar_t digits[16];
		int digitCount = 0;
		do
		{
			digits[digitCount++] = static_cast<wchar_t>(L'0' + pid % 10);
			pid /= 10;
		} while (pid != 0);
		while (digitCount > 0)
			*cursor++ = digits[--digitCount];
		*cursor = L'\0';

		g_mappingHandle =
		    OpenFileMappingW(FILE_MAP_ALL_ACCESS, FALSE, mappingName);
		if (!g_mappingHandle)
			return false;

		g_header = static_cast<VehCoverage::SharedHeader*>(
		    MapViewOfFile(g_mappingHandle, FILE_MAP_ALL_ACCESS, 0, 0, 0));
		if (!g_header)
			return false;

		if (g_header->magic != VehCoverage::ProtocolMagic ||
		    g_header->version != VehCoverage::ProtocolVersion)
		{
			SetLastError(ERROR_INVALID_DATA);
			Fail(g_header);
			return false;
		}

		g_probeCount = g_header->probeCount;
		g_rvas = VehCoverage::GetRvaArray(g_header);
		g_hits = VehCoverage::GetHitArray(g_header);
		g_maxRva = g_probeCount ? g_rvas[g_probeCount - 1] : 0;
		g_moduleBase =
		    reinterpret_cast<std::uint8_t*>(GetModuleHandleW(nullptr));
		if (!g_moduleBase)
		{
			Fail(g_header);
			return false;
		}

		if (g_probeCount != 0)
		{
			g_originalBytes = static_cast<std::uint8_t*>(
			    HeapAlloc(GetProcessHeap(), 0, g_probeCount));
			if (!g_originalBytes)
			{
				Fail(g_header);
				return false;
			}
		}

		// Arm the handler before planting any int3: if a later patch fails,
		// the probes already planted are still serviced, so the target can
		// never hit an unhandled breakpoint. Safe to order this way because
		// the target's only thread is suspended until the host resumes it.
		g_vehHandle = AddVectoredExceptionHandler(1, OnVectoredException);
		if (!g_vehHandle)
		{
			Fail(g_header);
			return false;
		}

		for (std::uint32_t i = 0; i < g_probeCount; ++i)
		{
			auto* address = g_moduleBase + g_rvas[i];

			// Leave the page writable so the hit-path byte restore needs no
			// syscall - the whole point of in-process servicing.
			DWORD oldProtect = 0;
			if (!VirtualProtect(
			        address, 1, PAGE_EXECUTE_READWRITE, &oldProtect))
			{
				Fail(g_header);
				return false;
			}

			g_originalBytes[i] = *address;
			*address = 0xCC;
		}

		if (g_probeCount != 0)
			FlushInstructionCache(GetCurrentProcess(), nullptr, 0);

		g_header->status =
		    static_cast<std::uint32_t>(VehCoverage::HelperStatus::Ready);
		return true;
	}
}

//-----------------------------------------------------------------------------
BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID) noexcept
{
	if (reason == DLL_PROCESS_ATTACH)
	{
		DisableThreadLibraryCalls(module);
		// A failure is reported through the shared header (status = Error);
		// the DLL still loads so the host can read the code and detach
		// cleanly rather than crashing the target.
		Initialize();
	}
	return TRUE;
}
