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

#include <memory>
#include <optional>
#include <vector>

#include <Windows.h>

#include "TaleaCppCovExport.hpp"

namespace TaleaCppCov
{
	// A source line's code range inside the scanned function.
	struct LineRunLine
	{
		DWORD64 virtualAddress_;
		std::uint32_t length_;
	};

	// A maximal straight-line instruction sequence: entered only at its
	// leader, and once entered every instruction up to and including its
	// final one executes. One breakpoint at the leader therefore proves
	// execution of every line the run touches.
	struct LineRun
	{
		DWORD64 leaderVirtualAddress_;
		std::vector<size_t> lineIndices_; // indices into the input lines
	};

	// Splits one function's code into straight-line runs so a single
	// breakpoint per run can replace a breakpoint per line.
	//
	// Soundness rules (no false coverage, no missed coverage):
	// - runs end at every branch, return, interrupt - and at every call,
	//   because a callee can throw and never return;
	// - every statically-known branch target starts a new run;
	// - a function containing an indirect jump (switch jump table: targets
	//   unknown statically) or undecodable bytes is rejected entirely, and
	//   the caller falls back to one breakpoint per line;
	// - runs never span functions (the caller scans one function at a
	//   time), so indirect calls and exception dispatch, which enter at
	//   function boundaries only, cannot enter a run mid-way.
	class TALEACPPCOV_DLL LineRunScanner
	{
	  public:
		explicit LineRunScanner(bool is64BitCode);
		~LineRunScanner();

		LineRunScanner(const LineRunScanner&) = delete;
		LineRunScanner& operator=(const LineRunScanner&) = delete;

		// buffer holds the function's original (unpatched) code bytes and
		// lines must be sorted by virtualAddress_. Returns the runs that
		// touch at least one line, or nullopt when the function cannot be
		// analyzed soundly.
		std::optional<std::vector<LineRun>>
		ComputeRuns(const std::uint8_t* buffer,
		            size_t size,
		            DWORD64 startVirtualAddress,
		            const std::vector<LineRunLine>& lines) const;

	  private:
		class Impl;
		std::unique_ptr<Impl> impl_;
	};
}
