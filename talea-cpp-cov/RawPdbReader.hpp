// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 natyamatsya and talea-cpp-cov contributors

#pragma once

#include <filesystem>

#include "TaleaCppCovExport.hpp"

namespace TaleaCppCov
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
	class TALEACPPCOV_DLL RawPdbReader
	{
	  public:
		static bool TryRead(const std::filesystem::path& binaryPath,
		                    ModuleLineTable& out);
	};
}
