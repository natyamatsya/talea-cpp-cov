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

#include <Windows.h>
#include <memory>
#include <vector>

#include "CppCoverageExport.hpp"
#include "BranchInstruction.hpp"

namespace CppCoverage
{
	// Finds conditional branch instructions (Jcc) in a code buffer
	// by disassembling it with Zydis.
	class CPPCOVERAGE_DLL BranchDisassembler
	{
	  public:
		explicit BranchDisassembler(bool is64BitCode);
		~BranchDisassembler();

		std::vector<BranchInstruction>
		FindConditionalBranches(const unsigned char* buffer,
		                        size_t size,
		                        DWORD64 startVirtualAddress) const;

	  private:
		BranchDisassembler(const BranchDisassembler&) = delete;
		BranchDisassembler& operator=(const BranchDisassembler&) = delete;

		class Impl;
		std::unique_ptr<Impl> impl_;
	};
}
