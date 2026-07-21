// OpenCppCoverage is an open source code coverage for C++.
// Copyright (C) 2014 OpenCppCoverage
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
#include "CppCoverageExport.hpp"

namespace CppCoverage
{
	class Address;

	class CPPCOVERAGE_DLL BreakPoint
	{
	  public:
		BreakPoint() = default;

		static const unsigned char breakPointInstruction;

		void RemoveBreakPoint(const Address&,
		                      unsigned char oldInstruction) const;

		using InstructionCollection =
		    std::vector<std::pair<unsigned char, DWORD64>>;

		InstructionCollection
		SetBreakPoints(HANDLE hProcess, std::vector<DWORD64>&& addresses) const;

		void SetBreakPoint(const Address&) const;

		void AdjustIpAfterBreakPointRemoval(HANDLE hThread) const;

		void AdjustIpAndSetTrapFlag(HANDLE hThread) const;

	  private:
		BreakPoint(const BreakPoint&) = delete;
		BreakPoint& operator=(const BreakPoint&) = delete;

		// Rewinds the thread's instruction pointer by one byte (past the int3),
		// optionally arming the trap flag to single-step the restored
		// instruction. Only the control registers are touched.
		void RewindInstructionPointer(HANDLE hThread, bool setTrapFlag) const;
	};
}
