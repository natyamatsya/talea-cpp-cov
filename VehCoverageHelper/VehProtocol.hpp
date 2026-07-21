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
namespace VehCoverage
{
	constexpr std::uint32_t ProtocolMagic = 0x43484556;   // "VEHC"
	constexpr std::uint32_t ProtocolVersion = 1;

	// Written by the helper so the host can tell setup succeeded before it
	// resumes the target's main thread.
	enum class HelperStatus : std::uint32_t
	{
		Initializing = 0,
		Ready = 1,
		Error = 2,
	};

	// Layout of the shared section:
	//   SharedHeader
	//   std::uint32_t rva[probeCount]   (module-relative, sorted ascending)
	//   std::uint8_t  hit[probeCount]   (helper writes 1 on first execution)
	// The host fills magic/version/probeCount and the rva[] array before
	// injection; the helper patches an int3 at each (mainModuleBase + rva)
	// and records hits; the host reads hit[] after the target exits.
	struct SharedHeader
	{
		std::uint32_t magic;
		std::uint32_t version;
		volatile std::uint32_t status;   // HelperStatus
		std::uint32_t probeCount;
		std::uint32_t helperErrorCode;   // GetLastError() when status == Error
		std::uint32_t reserved;
	};

	//-------------------------------------------------------------------------
	inline const std::uint32_t* GetRvaArray(const SharedHeader* header)
	{
		return reinterpret_cast<const std::uint32_t*>(header + 1);
	}

	//-------------------------------------------------------------------------
	inline std::uint8_t* GetHitArray(SharedHeader* header)
	{
		return reinterpret_cast<std::uint8_t*>(
		    reinterpret_cast<std::uint32_t*>(header + 1) + header->probeCount);
	}

	//-------------------------------------------------------------------------
	inline std::size_t GetSharedSize(std::uint32_t probeCount)
	{
		return sizeof(SharedHeader) +
		       static_cast<std::size_t>(probeCount) * sizeof(std::uint32_t) +
		       static_cast<std::size_t>(probeCount);
	}

	// The file-mapping name is this prefix followed by the target's decimal
	// process id, in the session-local namespace (host and target share a
	// session). Both sides derive the same name from the pid, so no other
	// channel is needed to hand it over.
	constexpr const wchar_t* MappingNamePrefix =
	    L"Local\\OpenCppCoverageVeh_";
}
