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
#include "VehBranchEval.hpp"

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
		VehCoverage::BranchProbe* branches_; // sorted by rva_, or null
		std::uint32_t branchCount_;
	};

	constexpr std::uint32_t MaxActiveModules = 4096;

	// --- Per-thread pending branch re-arm ------------------------------------
	// A branch probe is fire-many: when it hits, the helper restores the Jcc,
	// single-steps over it (trap flag), and re-plants the int3 on the following
	// single-step. This table remembers, per thread, which branch address to
	// re-plant. Fixed-size and lock-free so the handler never allocates; a
	// thread has at most one pending re-arm at a time (the single-step follows
	// the breakpoint immediately). If it overflows, that branch is simply not
	// re-armed - best-effort, like the line probes.
	struct PendingRearm
	{
		volatile LONG threadId_; // 0 = free slot
		std::uint8_t* address_;  // branch's absolute address to re-plant
	};
	constexpr int MaxPendingRearms = 256;
	PendingRearm g_pending[MaxPendingRearms];

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
	// Index of rva in a module's sorted BranchProbe array, or -1.
	std::int32_t FindBranchProbe(const VehCoverage::BranchProbe* branches,
	                             std::uint32_t branchCount,
	                             std::uint32_t rva) noexcept
	{
		std::uint32_t low = 0;
		std::uint32_t high = branchCount;
		while (low < high)
		{
			auto mid = low + (high - low) / 2;
			auto value = branches[mid].rva_;
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
	// Remembers that this thread must re-plant address on its next single step.
	void RememberPendingRearm(LONG threadId, std::uint8_t* address) noexcept
	{
		for (int i = 0; i < MaxPendingRearms; ++i)
		{
			if (InterlockedCompareExchange(
			        &g_pending[i].threadId_, threadId, 0) == 0)
			{
				g_pending[i].address_ = address;
				return;
			}
		}
		// Table full: skip the re-arm. The int3 stays removed, so this branch
		// simply stops reporting further outcomes (best-effort).
	}

	//-------------------------------------------------------------------------
	// Consumes and returns this thread's pending re-arm address, or null.
	std::uint8_t* TakePendingRearm(LONG threadId) noexcept
	{
		for (int i = 0; i < MaxPendingRearms; ++i)
		{
			if (g_pending[i].threadId_ == threadId)
			{
				auto* address = g_pending[i].address_;
				g_pending[i].address_ = nullptr;
				InterlockedExchange(&g_pending[i].threadId_, 0);
				return address;
			}
		}
		return nullptr;
	}

	//-------------------------------------------------------------------------
	// Services a branch probe: evaluate the jump direction from the trapped
	// thread's flags, restore the Jcc, and either retire it (both directions
	// seen) or single-step over it so the int3 can be re-planted. Returns true
	// if the address was a branch of this module.
	bool ServiceBranch(const ActiveModule& module,
	                   std::uint8_t* address,
	                   std::uint32_t rva,
	                   EXCEPTION_POINTERS* exceptionInfo) noexcept
	{
		// Branch coverage is 64-bit only - the host injects the helper only into
		// 64-bit targets - so the x64 context (Rip/Rcx/EFlags) is always here.
		auto branchIndex =
		    FindBranchProbe(module.branches_, module.branchCount_, rva);
		if (branchIndex < 0)
			return false;

		auto& branch = module.branches_[branchIndex];
		auto* context = exceptionInfo->ContextRecord;

		bool taken = VehCoverage::IsBranchTaken(
		    static_cast<VehCoverage::BranchCondition>(branch.condition_),
		    static_cast<std::uint32_t>(context->EFlags),
		    static_cast<std::uint64_t>(context->Rcx));
		branch.outcome_ |= taken ? VehCoverage::BranchTaken
		                         : VehCoverage::BranchFallThrough;

		// If this address is also a line probe, mark the line executed.
		auto lineIndex = FindProbe(module.rvas_, module.probeCount_, rva);
		if (lineIndex >= 0)
			module.hits_[lineIndex] = 1;

		// Restore the real Jcc and rewind so it re-executes.
		*address = branch.originalByte_;
		FlushInstructionCache(GetCurrentProcess(), address, 1);
		context->Rip = reinterpret_cast<DWORD64>(address);

		// Once both directions are seen the probe is retired (byte stays
		// restored). Otherwise single-step over the Jcc and re-arm afterwards.
		if ((branch.outcome_ &
		     (VehCoverage::BranchTaken | VehCoverage::BranchFallThrough)) !=
		    (VehCoverage::BranchTaken | VehCoverage::BranchFallThrough))
		{
			RememberPendingRearm(
			    static_cast<LONG>(GetCurrentThreadId()), address);
			context->EFlags |= 0x100; // trap flag: fault after the next insn
		}
		return true;
	}

	//-------------------------------------------------------------------------
	LONG CALLBACK OnVectoredException(EXCEPTION_POINTERS* exceptionInfo) noexcept
	{
		auto* record = exceptionInfo->ExceptionRecord;

		// A single-step is ours only if this thread has a pending branch
		// re-arm; otherwise it belongs to the app and must pass through.
		if (record->ExceptionCode == EXCEPTION_SINGLE_STEP)
		{
			auto* address =
			    TakePendingRearm(static_cast<LONG>(GetCurrentThreadId()));
			if (!address)
				return EXCEPTION_CONTINUE_SEARCH;

			*address = 0xCC; // re-arm the branch probe
			FlushInstructionCache(GetCurrentProcess(), address, 1);
			exceptionInfo->ContextRecord->EFlags &= ~0x100u; // clear trap flag
			return EXCEPTION_CONTINUE_EXECUTION;
		}

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

			// Branch probes are checked first: an address that is both a line
			// leader and a Jcc is serviced as a branch (which also marks the
			// line), matching the debugger engine.
			if (module.branchCount_ &&
			    ServiceBranch(module, address, rva, exceptionInfo))
				return EXCEPTION_CONTINUE_EXECUTION;

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

			exceptionInfo->ContextRecord->Rip =
			    reinterpret_cast<DWORD64>(address);
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

		auto branchCount = descriptor.branchCount_;
		auto* branches =
		    branchCount ? reinterpret_cast<VehCoverage::BranchProbe*>(
		                      sectionBase + descriptor.branchOffset_)
		                : nullptr;

		auto* originalBytes = static_cast<std::uint8_t*>(
		    HeapAlloc(GetProcessHeap(), 0, probeCount));
		if (!originalBytes)
			return;

		// The active range must cover both line probes and branch probes.
		auto maxRva = rvas[probeCount - 1];
		if (branchCount && branches[branchCount - 1].rva_ > maxRva)
			maxRva = branches[branchCount - 1].rva_;

		// Publish the active entry before planting any int3, so the handler
		// can service a probe the instant it is planted.
		auto slot = InterlockedIncrement(&g_activeReserved) - 1;
		if (slot < 0 || slot >= static_cast<LONG>(MaxActiveModules))
			return;

		g_active[slot].base_ = moduleBase;
		g_active[slot].end_ = moduleBase + maxRva + 1;
		g_active[slot].rvas_ = rvas;
		g_active[slot].hits_ = hits;
		g_active[slot].originalBytes_ = originalBytes;
		g_active[slot].probeCount_ = probeCount;
		g_active[slot].branches_ = branches;
		g_active[slot].branchCount_ = branchCount;
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

		// Plant the branch probes. Their original byte comes from the host, so
		// a branch sharing an address with a line probe (already 0xCC) is not
		// corrupted by re-reading the patched byte.
		for (std::uint32_t i = 0; i < branchCount; ++i)
		{
			auto* address =
			    const_cast<std::uint8_t*>(moduleBase) + branches[i].rva_;
			DWORD oldProtect = 0;
			if (!VirtualProtect(
			        address, 1, PAGE_EXECUTE_READWRITE, &oldProtect))
				continue;
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

	// Defined below with the child-process hook; hooks a newly loaded module's
	// CreateProcess IAT slots (no-op until the hooks are armed).
	void HookProcessCreationInModule(HMODULE module) noexcept;

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

		// A module loaded after injection also needs its CreateProcess IAT
		// hooked, so children it spawns are covered too.
		HookProcessCreationInModule(
		    static_cast<HMODULE>(const_cast<void*>(data->dllBase_)));
	}

	//-------------------------------------------------------------------------
	// --- child-process coverage: hook CreateProcess ------------------------
	using CreateProcessWFn = BOOL(WINAPI*)(
	    LPCWSTR, LPWSTR, LPSECURITY_ATTRIBUTES, LPSECURITY_ATTRIBUTES, BOOL,
	    DWORD, LPVOID, LPCWSTR, LPSTARTUPINFOW, LPPROCESS_INFORMATION);
	using CreateProcessAFn = BOOL(WINAPI*)(
	    LPCSTR, LPSTR, LPSECURITY_ATTRIBUTES, LPSECURITY_ATTRIBUTES, BOOL,
	    DWORD, LPVOID, LPCSTR, LPSTARTUPINFOA, LPPROCESS_INFORMATION);

	CreateProcessWFn g_realCreateProcessW = nullptr;
	CreateProcessAFn g_realCreateProcessA = nullptr;
	HANDLE g_childPending = nullptr;
	HANDLE g_childReady = nullptr;

	//-------------------------------------------------------------------------
	// Writes prefix followed by pid's decimal digits into dst.
	void BuildPidName(wchar_t* dst, const wchar_t* prefix, DWORD pid) noexcept
	{
		wchar_t* cursor = dst;
		for (const wchar_t* p = prefix; *p; ++p)
			*cursor++ = *p;
		wchar_t digits[16];
		int count = 0;
		do
		{
			digits[count++] = static_cast<wchar_t>(L'0' + pid % 10);
			pid /= 10;
		} while (pid != 0);
		while (count > 0)
			*cursor++ = digits[--count];
		*cursor = L'\0';
	}

	//-------------------------------------------------------------------------
	// Hands a freshly spawned, still-suspended child to the host (which injects
	// the helper into it) and then resumes it. One registration at a time: the
	// section has a single pid slot and event pair; childLock serializes it.
	// childLock is int32 in the header (no windows.h there), so cast to LONG*.
	void RegisterChildWithHost(DWORD childPid,
	                           HANDLE childThread,
	                           bool forcedSuspend) noexcept
	{
		auto* lock = reinterpret_cast<volatile LONG*>(&g_header->childLock);
		while (InterlockedCompareExchange(lock, 1, 0) != 0)
			Sleep(0);
		g_header->childPid = childPid;
		SetEvent(g_childPending);
		WaitForSingleObject(g_childReady, 30000);
		InterlockedExchange(lock, 0);

		if (forcedSuspend)
			ResumeThread(childThread);
	}

	//-------------------------------------------------------------------------
	BOOL WINAPI HookedCreateProcessW(
	    LPCWSTR applicationName,
	    LPWSTR commandLine,
	    LPSECURITY_ATTRIBUTES processAttributes,
	    LPSECURITY_ATTRIBUTES threadAttributes,
	    BOOL inheritHandles,
	    DWORD creationFlags,
	    LPVOID environment,
	    LPCWSTR currentDirectory,
	    LPSTARTUPINFOW startupInfo,
	    LPPROCESS_INFORMATION processInfo) noexcept
	{
		bool forcedSuspend = (creationFlags & CREATE_SUSPENDED) == 0;
		auto ok = g_realCreateProcessW(applicationName,
		                               commandLine,
		                               processAttributes,
		                               threadAttributes,
		                               inheritHandles,
		                               creationFlags | CREATE_SUSPENDED,
		                               environment,
		                               currentDirectory,
		                               startupInfo,
		                               processInfo);
		if (ok && processInfo)
			RegisterChildWithHost(processInfo->dwProcessId,
			                      processInfo->hThread,
			                      forcedSuspend);
		return ok;
	}

	//-------------------------------------------------------------------------
	BOOL WINAPI HookedCreateProcessA(
	    LPCSTR applicationName,
	    LPSTR commandLine,
	    LPSECURITY_ATTRIBUTES processAttributes,
	    LPSECURITY_ATTRIBUTES threadAttributes,
	    BOOL inheritHandles,
	    DWORD creationFlags,
	    LPVOID environment,
	    LPCSTR currentDirectory,
	    LPSTARTUPINFOA startupInfo,
	    LPPROCESS_INFORMATION processInfo) noexcept
	{
		bool forcedSuspend = (creationFlags & CREATE_SUSPENDED) == 0;
		auto ok = g_realCreateProcessA(applicationName,
		                               commandLine,
		                               processAttributes,
		                               threadAttributes,
		                               inheritHandles,
		                               creationFlags | CREATE_SUSPENDED,
		                               environment,
		                               currentDirectory,
		                               startupInfo,
		                               processInfo);
		if (ok && processInfo)
			RegisterChildWithHost(processInfo->dwProcessId,
			                      processInfo->hThread,
			                      forcedSuspend);
		return ok;
	}

	//-------------------------------------------------------------------------
	// Repoints every IAT slot in module that resolves to target at hook.
	// Matching by resolved address (not by import name) also catches bound
	// imports and needs no name-table walk.
	void HookIatSlots(HMODULE module, ULONG_PTR target, void* hook) noexcept
	{
		auto* base = reinterpret_cast<std::uint8_t*>(module);
		auto* dosHeader = reinterpret_cast<IMAGE_DOS_HEADER*>(base);
		if (dosHeader->e_magic != IMAGE_DOS_SIGNATURE)
			return;
		auto* ntHeaders =
		    reinterpret_cast<IMAGE_NT_HEADERS*>(base + dosHeader->e_lfanew);
		if (ntHeaders->Signature != IMAGE_NT_SIGNATURE)
			return;
		auto directory = ntHeaders->OptionalHeader
		                     .DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
		if (directory.VirtualAddress == 0)
			return;

		for (auto* import = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(
		         base + directory.VirtualAddress);
		     import->Name != 0;
		     ++import)
		{
			for (auto* thunk = reinterpret_cast<IMAGE_THUNK_DATA*>(
			         base + import->FirstThunk);
			     thunk->u1.Function != 0;
			     ++thunk)
			{
				if (thunk->u1.Function != target)
					continue;
				DWORD oldProtect = 0;
				if (VirtualProtect(&thunk->u1.Function,
				                   sizeof(void*),
				                   PAGE_READWRITE,
				                   &oldProtect))
				{
					thunk->u1.Function =
					    reinterpret_cast<ULONGLONG>(hook);
					VirtualProtect(&thunk->u1.Function,
					               sizeof(void*),
					               oldProtect,
					               &oldProtect);
				}
			}
		}
	}

	//-------------------------------------------------------------------------
	// Hooks both CreateProcessW and CreateProcessA in one module. A no-op until
	// the hooks are armed (g_realCreateProcessW set), so the DLL-load callback
	// can call it unconditionally.
	void HookProcessCreationInModule(HMODULE module) noexcept
	{
		if (!g_realCreateProcessW)
			return;
		HookIatSlots(module,
		             reinterpret_cast<ULONG_PTR>(g_realCreateProcessW),
		             reinterpret_cast<void*>(&HookedCreateProcessW));
		if (g_realCreateProcessA)
			HookIatSlots(module,
			             reinterpret_cast<ULONG_PTR>(g_realCreateProcessA),
			             reinterpret_cast<void*>(&HookedCreateProcessA));
	}

	//-------------------------------------------------------------------------
	// Hooks every currently-loaded module. Safe under the loader lock: it only
	// reads the loader's module list and patches IAT memory (no allocation, no
	// loader calls). Modules loaded later are hooked from OnDllLoaded.
	void HookAllLoadedModules() noexcept
	{
		auto* peb = NtCurrentTeb()->ProcessEnvironmentBlock;
		if (!peb || !peb->Ldr)
			return;
		auto* head = &peb->Ldr->InMemoryOrderModuleList;
		for (auto* cursor = head->Flink; cursor != head; cursor = cursor->Flink)
		{
			auto* entry = CONTAINING_RECORD(
			    cursor, LDR_DATA_TABLE_ENTRY, InMemoryOrderLinks);
			if (entry->DllBase)
				HookProcessCreationInModule(
				    static_cast<HMODULE>(entry->DllBase));
		}
	}

	//-------------------------------------------------------------------------
	// If the host asked for child coverage, resolve the real CreateProcess
	// functions, open the host's registration events, and install the hooks.
	void InstallChildProcessHook() noexcept
	{
		if (!g_header->coverChildren)
			return;

		auto* kernel32 = GetModuleHandleW(L"kernel32.dll");
		g_realCreateProcessW = reinterpret_cast<CreateProcessWFn>(
		    GetProcAddress(kernel32, "CreateProcessW"));
		g_realCreateProcessA = reinterpret_cast<CreateProcessAFn>(
		    GetProcAddress(kernel32, "CreateProcessA"));
		if (!g_realCreateProcessW)
			return;

		auto pid = GetCurrentProcessId();
		wchar_t name[128];
		BuildPidName(name, VehCoverage::ChildPendingEventPrefix, pid);
		g_childPending = OpenEventW(EVENT_MODIFY_STATE, FALSE, name);
		BuildPidName(name, VehCoverage::ChildReadyEventPrefix, pid);
		g_childReady = OpenEventW(SYNCHRONIZE, FALSE, name);
		if (!g_childPending || !g_childReady)
		{
			g_realCreateProcessW = nullptr; // stay dormant if setup failed
			g_realCreateProcessA = nullptr;
			return;
		}

		HookAllLoadedModules();
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

		// Hook CreateProcess so children are instrumented too, if requested.
		// Modules mapped at injection are hooked here; modules loaded later are
		// hooked from OnDllLoaded.
		InstallChildProcessHook();

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
