// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2014 OpenCppCoverage
// SPDX-FileCopyrightText: 2026 natyamatsya and talea-cpp-cov contributors

#pragma once

#include <filesystem>
#include <vector>

#include "TaleaCppCovExport.hpp"

namespace TaleaCppCov
{
	// Absolute paths of every module in the executable's recursive static-import
	// closure that lives next to the executable - the app's own DLLs the loader
	// will map at startup, the ones that carry the project's debug info. Imports
	// resolved outside the executable's directory (system DLLs) are skipped: they
	// are not instrumented and have no relevant PDBs, and replicating the loader's
	// full search order is unnecessary for warming the PDB cache. Deduped by base
	// name; the executable itself is not included. Best-effort - a malformed or
	// unreadable PE simply contributes nothing.
	TALEACPPCOV_DLL std::vector<std::filesystem::path>
	ComputeImportClosure(const std::filesystem::path& executablePath);
}
