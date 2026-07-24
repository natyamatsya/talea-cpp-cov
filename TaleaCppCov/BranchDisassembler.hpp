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

#include <Windows.h>
#include <memory>
#include <optional>
#include <vector>

#include "TaleaCppCovExport.hpp"
#include "BranchInstruction.hpp"

namespace TaleaCppCov
{
	// Finds conditional branch instructions (Jcc) in a code buffer
	// by disassembling it with Zydis.
	class TALEACPPCOV_DLL BranchDisassembler
	{
	  public:
		explicit BranchDisassembler(bool is64BitCode);
		~BranchDisassembler();

		// Disassembles a whole function's code range and returns its
		// conditional branches. Returns std::nullopt when the range cannot be
		// proven to be pure code: any decode failure or any indirect jump
		// rejects the whole function, the same rule LineRunScanner applies.
		//
		// The rejection is what makes patching safe: MSVC stores switch jump
		// tables INLINE in .text (a DWORD target table plus a BYTE index
		// table, inside the function's range and covered by its PDB line
		// records). Sweeping those data bytes finds fake "Jcc"s, and a branch
		// breakpoint written into a jump table corrupts the values the
		// dispatch sequence later reads - a wild jump in the profiled
		// program. Every jump-table function necessarily contains an indirect
		// jmp, so rejecting on indirection catches exactly that case.
		std::optional<std::vector<BranchInstruction>>
		FindConditionalBranches(const std::uint8_t* buffer,
		                        size_t size,
		                        DWORD64 startVirtualAddress) const;

	  private:
		BranchDisassembler(const BranchDisassembler&) = delete;
		BranchDisassembler& operator=(const BranchDisassembler&) = delete;

		class Impl;
		std::unique_ptr<Impl> impl_;
	};
}
