// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 natyamatsya and talea-cpp-cov contributors

#pragma once
#include <cstdint>

#include <filesystem>
#include <optional>
#include <string>

#include <Windows.h>

#include "TaleaCppCovExport.hpp"

namespace TaleaCppCov
{
	// The linker's exact identity of a module's debug information: the PDB
	// GUID and age from the PE debug directory (the same key symbol servers
	// use). Two binaries share a signature if and only if they came from the
	// same link, so anything derived from the PDB can be cached under it.
	struct TALEACPPCOV_DLL PdbSignature
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
	TALEACPPCOV_DLL std::optional<PdbSignature>
	ExtractPdbSignature(const std::filesystem::path& binaryPath);
}
