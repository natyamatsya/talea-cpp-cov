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

#pragma once

#include <cstddef>
#include <cstdint>

// Shared contract between the OpenCppCoverage host and the injected VEH
// helper DLL. Header-only, no dependencies, so the helper stays a tiny
// self-contained DLL.
//
// The host precomputes a probe spec for every instrumented module (the main
// executable plus filter-matching DLLs with debug information) and hands it
// over in one file mapping. The helper patches each module's probes at that
// module's load-time base - the main executable and any DLL already mapped at
// injection immediately, later DLLs as the loader reports them - records first
// hits in the module's bitmap, and the host reads the bitmaps after the target
// exits.
namespace VehCoverage
{
	constexpr std::uint32_t ProtocolMagic = 0x43484556;   // "VEHC"
	constexpr std::uint32_t ProtocolVersion = 4;

	// Maximum wide chars (including the null) for a module's base name in the
	// descriptor. Base names longer than this are not instrumented.
	constexpr std::size_t MaxModuleNameChars = 128;

	// Maximum wide chars (including the null) for a module's full load path,
	// which the helper reports back so coverage names the actually-loaded file.
	constexpr std::size_t MaxModulePathChars = 260;

	// Written by the helper so the host can tell setup succeeded before it
	// resumes the target's main thread.
	enum class HelperStatus : std::uint32_t
	{
		Initializing = 0,
		Ready = 1,
		Error = 2,
	};

	// One conditional-branch probe (only present when --branch_coverage is on).
	// The host supplies the original opcode byte and the jump condition; the
	// helper single-steps the Jcc and records which directions were seen in
	// outcome_. rva_-sorted within a module so the helper can binary-search it.
	struct BranchProbe
	{
		std::uint32_t rva_;
		std::uint8_t originalByte_;      // the Jcc's first byte, from the host
		std::uint8_t condition_;         // VehCoverage::BranchCondition value
		volatile std::uint8_t outcome_;  // helper writes: bit0 taken, bit1 fall-through
		std::uint8_t reserved_;
	};

	// One instrumented module. rvaOffset / hitOffset are byte offsets from the
	// start of the shared section to this module's uint32 rva[probeCount] and
	// uint8 hit[probeCount] arrays. branchOffset points to BranchProbe
	// [branchCount] (both zero when branch coverage is off). The helper sets
	// loaded = 1 when it patches the module, so the host reports coverage only
	// for modules that actually mapped.
	struct ModuleDescriptor
	{
		wchar_t baseName_[MaxModuleNameChars]; // lowercased, e.g. "mylib.dll"
		wchar_t loadedPath_[MaxModulePathChars]; // helper writes the load path
		std::uint32_t probeCount_;
		std::uint32_t rvaOffset_;
		std::uint32_t hitOffset_;
		std::uint32_t branchCount_;
		std::uint32_t branchOffset_;
		volatile std::uint32_t loaded_;
	};

	// Layout of the shared section:
	//   SharedHeader
	//   ModuleDescriptor[moduleCount]
	//   (per module, at rvaOffset_) std::uint32_t rva[probeCount_]
	//   (per module, at hitOffset_) std::uint8_t  hit[probeCount_]
	struct SharedHeader
	{
		std::uint32_t magic;
		std::uint32_t version;
		volatile std::uint32_t status;   // HelperStatus
		std::uint32_t moduleCount;
		std::uint32_t helperErrorCode;   // GetLastError() when status == Error
		std::uint32_t reserved;

		// --- child-process coverage (experimental) ---------------------------
		// When coverChildren is 1 the helper hooks CreateProcess and, for each
		// child, force-suspends it, publishes its pid here, signals the host's
		// pending event, and waits on the ready event while the host injects the
		// helper into the child. childLock serializes concurrent spawns onto the
		// single childPid slot. Set stopChildThread to unblock the host's
		// registration thread at shutdown.
		std::uint32_t coverChildren;
		volatile std::int32_t childLock;      // 0 = free
		volatile std::uint32_t childPid;      // helper writes the spawned pid
		volatile std::uint32_t stopChildThread;
	};

	// Session-local named auto-reset events for child registration; each is
	// followed by the *spawning* process's decimal pid (so a process's own
	// helper and the host agree on the names without another channel).
	constexpr const wchar_t* ChildPendingEventPrefix =
	    L"Local\\OpenCppCoverageVehChildPending_";
	constexpr const wchar_t* ChildReadyEventPrefix =
	    L"Local\\OpenCppCoverageVehChildReady_";

	//-------------------------------------------------------------------------
	// ASCII lowercase, used by both host and helper so module base names match
	// without depending on a locale (filenames are ASCII in practice).
	inline wchar_t AsciiToLower(wchar_t c)
	{
		return (c >= L'A' && c <= L'Z')
		           ? static_cast<wchar_t>(c - L'A' + L'a')
		           : c;
	}

	//-------------------------------------------------------------------------
	inline ModuleDescriptor* GetModuleDescriptors(SharedHeader* header)
	{
		return reinterpret_cast<ModuleDescriptor*>(header + 1);
	}

	//-------------------------------------------------------------------------
	inline std::size_t DescriptorsSize(std::uint32_t moduleCount)
	{
		return static_cast<std::size_t>(moduleCount) *
		       sizeof(ModuleDescriptor);
	}

	//-------------------------------------------------------------------------
	// Byte size of a module's rva[] + hit[] arrays.
	inline std::size_t ModuleArraysSize(std::uint32_t probeCount)
	{
		return static_cast<std::size_t>(probeCount) * sizeof(std::uint32_t) +
		       static_cast<std::size_t>(probeCount);
	}

	//-------------------------------------------------------------------------
	// Byte size of a module's BranchProbe[] array.
	inline std::size_t BranchArraysSize(std::uint32_t branchCount)
	{
		return static_cast<std::size_t>(branchCount) * sizeof(BranchProbe);
	}

	// The file-mapping name is this prefix followed by the target's decimal
	// process id, in the session-local namespace (host and target share a
	// session). Both sides derive the same name from the pid, so no other
	// channel is needed to hand it over.
	constexpr const wchar_t* MappingNamePrefix =
	    L"Local\\OpenCppCoverageVeh_";
}
