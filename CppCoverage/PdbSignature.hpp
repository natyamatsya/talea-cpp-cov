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
#include <cstdint>

#include <filesystem>
#include <optional>
#include <string>

#include <Windows.h>

#include "CppCoverageExport.hpp"

namespace CppCoverage
{
	// The linker's exact identity of a module's debug information: the PDB
	// GUID and age from the PE debug directory (the same key symbol servers
	// use). Two binaries share a signature if and only if they came from the
	// same link, so anything derived from the PDB can be cached under it.
	struct CPPCOVERAGE_DLL PdbSignature
	{
		GUID guid_{};
		std::uint32_t age_ = 0;

		// The PDB path recorded in the binary (build-machine path). Used to
		// locate the PDB for the DIA-free reader; not part of the identity.
		std::wstring pdbPath_;

		// Filesystem- and map-friendly form: 32 hex digits + "-" + age.
		// Uses only the GUID and age, never the path.
		std::wstring ToString() const;
	};

	// Reads the signature from the CodeView (RSDS) entry of the binary's
	// debug directory, without loading DIA. Returns nullopt when the file
	// is not a PE or carries no CodeView debug information.
	CPPCOVERAGE_DLL std::optional<PdbSignature>
	ExtractPdbSignature(const std::filesystem::path& binaryPath);
}
