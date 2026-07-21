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

#include "CppCoverageExport.hpp"

namespace CppCoverage
{
	struct ModuleLineTable;

	// A DIA-free PDB line-table reader built on raw_pdb: it memory-maps the PDB
	// and scans the columnar CodeView streams directly, orders of magnitude
	// faster than DIA's per-record COM calls, and - being lock-free immutable
	// data - parses each compiland's line stream in parallel.
	//
	// It produces the same ModuleLineTable as the DIA path (raw source-file
	// names, line records with RVA and code length, sorted function ranges), so
	// the cache and everything downstream are unchanged. It returns false -
	// leaving the caller to fall back to DIA - when the PDB cannot be found or
	// mapped, does not match the binary, was linked /DEBUG:FASTLINK, or has no
	// names stream. Function names are not resolved here (the fast-mode
	// instantiation axis keeps using DIA for those).
	class CPPCOVERAGE_DLL RawPdbReader
	{
	  public:
		static bool TryRead(const std::filesystem::path& binaryPath,
		                    ModuleLineTable& out);
	};
}
