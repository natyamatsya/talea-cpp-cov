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

// On-disk format of a "fast mode" coverage dump, written by the runtime that
// the instrumented target links (see OpenCppCoverageFastCov.cpp) and read by
// the host-side importer.
//
// The target is built with clang-cl -fsanitize-coverage=inline-8bit-counters,
// pc-table and linked with lld. Each instrumented basic block gets a saturating
// 8-bit execution counter and a parallel PC-table entry (address + function-
// entry flag). The dump records, per module, every basic block's module-
// relative address, its execution count, and whether it is a function entry;
// the importer maps those back to source lines and template instantiations
// through the PDB.
namespace FastCov
{
	constexpr std::uint32_t DumpMagic = 0x4346434F;   // "OCFC"
	constexpr std::uint32_t DumpVersion = 1;

	// Wide chars (including the null) reserved for a module path. Longer paths
	// are truncated.
	constexpr std::uint32_t MaxModulePathChars = 260;

	// The environment variable the host sets to tell the runtime where to write
	// the dump. When unset, the runtime writes nothing.
	constexpr const wchar_t* OutputPathEnvVar = L"OPENCPPCOVERAGE_FASTCOV_OUTPUT";

#pragma pack(push, 1)
	struct DumpHeader
	{
		std::uint32_t magic_;
		std::uint32_t version_;
		std::uint32_t moduleCount_;
		std::uint32_t basicBlockCount_;
	};

	struct ModuleEntry
	{
		wchar_t path_[MaxModulePathChars];
	};

	struct BasicBlockRecord
	{
		std::uint32_t moduleIndex_;
		std::uint32_t rva_;
		std::uint8_t count_;      // saturating 8-bit execution count
		std::uint8_t isEntry_;    // 1 if a function entry block
		std::uint16_t reserved_;
	};
#pragma pack(pop)

	// Layout: DumpHeader, then ModuleEntry[moduleCount_], then
	// BasicBlockRecord[basicBlockCount_].
}
