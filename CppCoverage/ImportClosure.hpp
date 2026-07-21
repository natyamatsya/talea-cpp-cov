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

#include <filesystem>
#include <vector>

#include "CppCoverageExport.hpp"

namespace CppCoverage
{
	// Absolute paths of every module in the executable's recursive static-import
	// closure that lives next to the executable - the app's own DLLs the loader
	// will map at startup, the ones that carry the project's debug info. Imports
	// resolved outside the executable's directory (system DLLs) are skipped: they
	// are not instrumented and have no relevant PDBs, and replicating the loader's
	// full search order is unnecessary for warming the PDB cache. Deduped by base
	// name; the executable itself is not included. Best-effort - a malformed or
	// unreadable PE simply contributes nothing.
	CPPCOVERAGE_DLL std::vector<std::filesystem::path>
	ComputeImportClosure(const std::filesystem::path& executablePath);
}
