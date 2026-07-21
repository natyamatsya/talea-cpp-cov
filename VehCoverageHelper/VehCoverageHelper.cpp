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
// The host injects this DLL into the target (launched suspended) and creates a
// shared section describing every instrumented module (the main executable and
// filter-matching DLLs) with its probe RVAs. The helper installs a Vectored
// Exception Handler, registers a DLL-load notification, and plants an int3 at
// each probe of a module at that module's load-time base: modules already
// mapped at injection (the main executable) immediately, later DLLs as the
// loader reports them. When a probe first executes, the VEH records the hit in
// the module's shared bitmap and restores the original byte, all in process: a
// usermode exception dispatch (microseconds) instead of the cross-process
// debug-event round trip (~100 microseconds) the debugger engine pays per hit.
//
// Deliberately tiny and dependency-free (kernel32 + one ntdll import, no CRT
// startup, no PCH) so it is safe to run from DllMain under the loader lock.

#include <Windows.h>
#include <winternl.h>

#include "VehProtocol.hpp"

namespace
{
	// --- DLL-load notification (undocumented ntdll, stable since Vista) ------
	struct LdrDllNotificationData
	{
		ULONG flags_;
		const UNICODE_STRING* fullDllName_;
		const UNICODE_STRING* baseDllName_;
		PVOID dllBase_;
		ULONG sizeOfImage_;
	};
	constexpr ULONG LdrDllNotificationReasonLoaded = 1;
	using LdrDllNotificationFunction = VOID(CALLBACK*)(
	    ULONG, const LdrDllNotificationData*, PVOID);
	using LdrRegisterDllNotificationFn = NTSTATUS(NTAPI*)(
	    ULONG, LdrDllNotificationFunction, PVOID, PVOID*);

	// --- A module patched in this process ------------------------------------
	struct ActiveModule
	{
		const std::uint8_t* base_;
		const std::uint8_t* end_;      // base_ + maxRva + 1
		const std::uint32_t* rvas_;
		std::uint8_t* hits_;
		std::uint8_t* originalBytes_;
		std::uint32_t probeCount_;
	};

	constexpr std::uint32_t MaxActiveModules = 4096;

	VehCoverage::SharedHeader* g_header = nullptr;
	VehCoverage::ModuleDescriptor* g_descriptors = nullptr;
	std::uint32_t g_moduleCount = 0;
	HANDLE g_mappingHandle = nullptr;
	PVOID g_vehHandle = nullptr;
	PVOID g_notificationCookie = nullptr;

	// One claim flag per descriptor: the first thread to claim a module patches
	// it, so the attach-time scan and the load notification never collide.
	LONG g_claimed[MaxActiveModules] = {};

	ActiveModule g_active[MaxActiveModules];
	volatile LONG g_activeReserved = 0;
	volatile LONG g_activeReady[MaxActiveModules] = {};

	//-------------------------------------------------------------------------
	std::size_t NameLength(const wchar_t* name) noexcept
	{
		std::size_t length = 0;
		while (name[length])
			++length;
		return length;
	}

	//-------------------------------------------------------------------------
	// Index of rva in a module's sorted rva array, or -1. Runs on every int3,
	// so it stays allocation- and lock-free.
	std::int32_t FindProbe(const std::uint32_t* rvas,
	                       std::uint32_t probeCount,
	                       std::uint32_t rva) noexcept
	{
		std::uint32_t low = 0;
		std::uint32_t high = probeCount;
		while (low < high)
		{
			auto mid = low + (high - low) / 2;
			auto value = rvas[mid];
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

		auto reserved = g_activeReserved;
		for (LONG i = 0; i < reserved; ++i)
		{
			if (!g_activeReady[i])
				continue;
			const auto& module = g_active[i];
			if (address < module.base_ || address >= module.end_)
				continue;

			auto rva =
			    static_cast<std::uint32_t>(address - module.base_);
			auto index =
			    FindProbe(module.rvas_, module.probeCount_, rva);
			if (index < 0)
				continue; // not a probe of this module

			// Record the hit and retire the probe: restore the original byte
			// and rewind the instruction pointer so the real instruction
			// re-executes. Concurrent hits write the same byte and bit, so no
			// synchronization is needed (best-effort, like the debugger).
			module.hits_[index] = 1;
			*address = module.originalBytes_[index];
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

		return EXCEPTION_CONTINUE_SEARCH; // the app's own int3
	}

	//-------------------------------------------------------------------------
	// Plants the probes of descriptor descriptorIndex at moduleBase. Safe to
	// call from any thread; the first caller to claim the descriptor wins.
	void PatchModule(std::uint32_t descriptorIndex,
	                 const std::uint8_t* moduleBase) noexcept
	{
		if (InterlockedCompareExchange(&g_claimed[descriptorIndex], 1, 0) != 0)
			return;

		auto& descriptor = g_descriptors[descriptorIndex];
		// Report the actual load path so coverage names the loaded file, not
		// whichever copy the host found while scanning the directory.
		GetModuleFileNameW(reinterpret_cast<HMODULE>(
		                       const_cast<std::uint8_t*>(moduleBase)),
		                   descriptor.loadedPath_,
		                   VehCoverage::MaxModulePathChars);
		descriptor.loaded_ = 1;

		auto probeCount = descriptor.probeCount_;
		if (probeCount == 0)
			return;

		auto* sectionBase = reinterpret_cast<std::uint8_t*>(g_header);
		auto* rvas = reinterpret_cast<const std::uint32_t*>(
		    sectionBase + descriptor.rvaOffset_);
		auto* hits = sectionBase + descriptor.hitOffset_;

		auto* originalBytes = static_cast<std::uint8_t*>(
		    HeapAlloc(GetProcessHeap(), 0, probeCount));
		if (!originalBytes)
			return;

		// Publish the active entry before planting any int3, so the handler
		// can service a probe the instant it is planted.
		auto slot = InterlockedIncrement(&g_activeReserved) - 1;
		if (slot < 0 || slot >= static_cast<LONG>(MaxActiveModules))
			return;

		g_active[slot].base_ = moduleBase;
		g_active[slot].end_ = moduleBase + rvas[probeCount - 1] + 1;
		g_active[slot].rvas_ = rvas;
		g_active[slot].hits_ = hits;
		g_active[slot].originalBytes_ = originalBytes;
		g_active[slot].probeCount_ = probeCount;
		MemoryBarrier();
		g_activeReady[slot] = 1;

		for (std::uint32_t i = 0; i < probeCount; ++i)
		{
			auto* address = const_cast<std::uint8_t*>(moduleBase) + rvas[i];

			// Record the original before planting, so a probe firing mid-loop
			// finds its byte. Skip probes that cannot be made writable rather
			// than aborting the whole module (robustness over completeness).
			DWORD oldProtect = 0;
			if (!VirtualProtect(
			        address, 1, PAGE_EXECUTE_READWRITE, &oldProtect))
			{
				originalBytes[i] = 0xCC; // never restored: probe not planted
				continue;
			}
			originalBytes[i] = *address;
			*address = 0xCC;
		}
		FlushInstructionCache(GetCurrentProcess(), nullptr, 0);
	}

	//-------------------------------------------------------------------------
	// Case-insensitive (ASCII) match of a loaded module's base name against a
	// descriptor. The host lowercases descriptor names the same way.
	bool NameMatches(const wchar_t* descriptorName,
	                 const wchar_t* loadedName,
	                 std::size_t loadedLength) noexcept
	{
		std::size_t i = 0;
		for (; i < loadedLength && descriptorName[i]; ++i)
		{
			if (VehCoverage::AsciiToLower(loadedName[i]) != descriptorName[i])
				return false;
		}
		return i == loadedLength && descriptorName[i] == L'\0';
	}

	//-------------------------------------------------------------------------
	void PatchByName(const wchar_t* loadedName,
	                 std::size_t loadedLength,
	                 const std::uint8_t* moduleBase) noexcept
	{
		for (std::uint32_t i = 0; i < g_moduleCount; ++i)
		{
			if (NameMatches(g_descriptors[i].baseName_, loadedName,
			                loadedLength))
			{
				PatchModule(i, moduleBase);
				return;
			}
		}
	}

	//-------------------------------------------------------------------------
	VOID CALLBACK OnDllLoaded(ULONG reason,
	                          const LdrDllNotificationData* data,
	                          PVOID) noexcept
	{
		if (reason != LdrDllNotificationReasonLoaded || !data ||
		    !data->baseDllName_ || !data->baseDllName_->Buffer)
			return;

		PatchByName(data->baseDllName_->Buffer,
		            data->baseDllName_->Length / sizeof(wchar_t),
		            static_cast<const std::uint8_t*>(data->dllBase_));
	}

	//-------------------------------------------------------------------------
	void Fail() noexcept
	{
		if (g_header)
		{
			g_header->helperErrorCode = GetLastError();
			g_header->status =
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
		    g_header->version != VehCoverage::ProtocolVersion ||
		    g_header->moduleCount > MaxActiveModules)
		{
			SetLastError(ERROR_INVALID_DATA);
			Fail();
			return false;
		}

		g_moduleCount = g_header->moduleCount;
		g_descriptors = VehCoverage::GetModuleDescriptors(g_header);

		// Arm the handler before planting any int3.
		g_vehHandle = AddVectoredExceptionHandler(1, OnVectoredException);
		if (!g_vehHandle)
		{
			Fail();
			return false;
		}

		// Register for future DLL loads before scanning the already-loaded
		// modules, so a module mapping concurrently cannot slip through the
		// gap (double patches are prevented by the per-descriptor claim).
		auto* ntdll = GetModuleHandleW(L"ntdll.dll");
		auto registerNotification =
		    ntdll ? reinterpret_cast<LdrRegisterDllNotificationFn>(
		                GetProcAddress(ntdll, "LdrRegisterDllNotification"))
		          : nullptr;
		if (registerNotification)
		{
			registerNotification(
			    0, OnDllLoaded, nullptr, &g_notificationCookie);
		}

		// Patch every module already mapped (the main executable, plus any DLL
		// present at injection). Match by the descriptor's base name.
		for (std::uint32_t i = 0; i < g_moduleCount; ++i)
		{
			auto* moduleBase = reinterpret_cast<const std::uint8_t*>(
			    GetModuleHandleW(g_descriptors[i].baseName_));
			if (moduleBase)
				PatchModule(i, moduleBase);
		}

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
		// the DLL still loads so the host can detach cleanly rather than
		// crashing the target.
		Initialize();
	}
	return TRUE;
}
