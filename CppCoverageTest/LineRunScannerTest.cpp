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

#include "stdafx.h"
#include <cstdint>

#include "CppCoverage/LineRunScanner.hpp"

#include "TestTools.hpp"
#include "TestCoverageConsole/TestCoverageConsole.hpp"
#include "TestHelper/CoverageDataComparer.hpp"
#include "Plugin/Exporter/CoverageData.hpp"

namespace CppCoverageTest
{
	namespace
	{
		constexpr DWORD64 StartAddress = 0x1000;

		//---------------------------------------------------------------------
		std::optional<std::vector<CppCoverage::LineRun>>
		ComputeRuns(const std::vector<std::uint8_t>& code,
		            const std::vector<CppCoverage::LineRunLine>& lines)
		{
			CppCoverage::LineRunScanner scanner{true};
			return scanner.ComputeRuns(
			    code.data(), code.size(), StartAddress, lines);
		}

		//---------------------------------------------------------------------
		CppCoverage::LineRunLine LineAt(DWORD64 offset, std::uint32_t length)
		{
			return {StartAddress + offset, length};
		}
	}

	//-------------------------------------------------------------------------
	TEST(LineRunScannerTest, StraightLineCodeIsOneRun)
	{
		// inc rax; inc rax; inc rax; ret
		std::vector<std::uint8_t> code = {
		    0x48, 0xFF, 0xC0, 0x48, 0xFF, 0xC0, 0x48, 0xFF, 0xC0, 0xC3};
		auto runs =
		    ComputeRuns(code, {LineAt(0, 3), LineAt(3, 3), LineAt(6, 4)});

		ASSERT_TRUE(runs.has_value());
		ASSERT_EQ(1, runs->size());
		ASSERT_EQ(StartAddress, (*runs)[0].leaderVirtualAddress_);
		ASSERT_EQ((std::vector<size_t>{0, 1, 2}), (*runs)[0].lineIndices_);
	}

	//-------------------------------------------------------------------------
	TEST(LineRunScannerTest, ConditionalBranchSplitsRuns)
	{
		// 0: je +2 (target 4); 2: nop; 3: nop; 4: nop; 5: ret
		// Runs: [0,2) jcc-terminated, [2,4) ends before the target,
		// [4,6) from the target to the ret.
		std::vector<std::uint8_t> code = {0x74, 0x02, 0x90, 0x90, 0x90, 0xC3};
		auto runs = ComputeRuns(
		    code, {LineAt(0, 2), LineAt(2, 2), LineAt(4, 2)});

		ASSERT_TRUE(runs.has_value());
		ASSERT_EQ(3, runs->size());
		ASSERT_EQ(StartAddress, (*runs)[0].leaderVirtualAddress_);
		ASSERT_EQ(StartAddress + 2, (*runs)[1].leaderVirtualAddress_);
		ASSERT_EQ(StartAddress + 4, (*runs)[2].leaderVirtualAddress_);
		ASSERT_EQ((std::vector<size_t>{0}), (*runs)[0].lineIndices_);
		ASSERT_EQ((std::vector<size_t>{1}), (*runs)[1].lineIndices_);
		ASSERT_EQ((std::vector<size_t>{2}), (*runs)[2].lineIndices_);
	}

	//-------------------------------------------------------------------------
	TEST(LineRunScannerTest, CallEndsRun)
	{
		// A callee can throw: the line after the call must not be provable
		// from the run leader alone.
		// 0: call +0 (next instruction); 5: nop; 6: ret
		std::vector<std::uint8_t> code = {
		    0xE8, 0x00, 0x00, 0x00, 0x00, 0x90, 0xC3};
		auto runs = ComputeRuns(code, {LineAt(0, 5), LineAt(5, 2)});

		ASSERT_TRUE(runs.has_value());
		ASSERT_EQ(2, runs->size());
		ASSERT_EQ(StartAddress, (*runs)[0].leaderVirtualAddress_);
		ASSERT_EQ(StartAddress + 5, (*runs)[1].leaderVirtualAddress_);
	}

	//-------------------------------------------------------------------------
	TEST(LineRunScannerTest, LineSpanningTwoRunsBelongsToBoth)
	{
		// One line covering both sides of a call: entering either run
		// executes part of the line.
		std::vector<std::uint8_t> code = {
		    0xE8, 0x00, 0x00, 0x00, 0x00, 0x90, 0xC3};
		auto runs = ComputeRuns(code, {LineAt(0, 7)});

		ASSERT_TRUE(runs.has_value());
		ASSERT_EQ(2, runs->size());
		ASSERT_EQ((std::vector<size_t>{0}), (*runs)[0].lineIndices_);
		ASSERT_EQ((std::vector<size_t>{0}), (*runs)[1].lineIndices_);
	}

	//-------------------------------------------------------------------------
	TEST(LineRunScannerTest, IndirectJumpRejectsFunction)
	{
		// jmp rax: a jump table's targets are unknown statically.
		std::vector<std::uint8_t> code = {0xFF, 0xE0, 0x90, 0xC3};
		ASSERT_FALSE(ComputeRuns(code, {LineAt(2, 2)}).has_value());
	}

	//-------------------------------------------------------------------------
	TEST(LineRunScannerTest, UndecodableCodeRejectsFunction)
	{
		// A truncated call instruction cannot be decoded.
		std::vector<std::uint8_t> code = {0xE8, 0x00};
		ASSERT_FALSE(ComputeRuns(code, {LineAt(0, 2)}).has_value());
	}

	//-------------------------------------------------------------------------
	TEST(LineRunScannerTest, RunsWithoutLinesAreDropped)
	{
		// Only the code after the jump target carries a line.
		std::vector<std::uint8_t> code = {0x74, 0x02, 0x90, 0x90, 0x90, 0xC3};
		auto runs = ComputeRuns(code, {LineAt(4, 2)});

		ASSERT_TRUE(runs.has_value());
		ASSERT_EQ(1, runs->size());
		ASSERT_EQ(StartAddress + 4, (*runs)[0].leaderVirtualAddress_);
	}

	//-------------------------------------------------------------------------
	TEST(LineRunScannerTest, MergedCoverageMatchesPerLineCoverage)
	{
		// Merging is a pure performance optimization: a real coverage run
		// must produce exactly the same coverage data with and without it.
		// Select every TestCoverageConsole source: executed and unexecuted
		// lines, plain code and exception paths all take part.
		TestTools::CoverageArgs args{
		    {TestCoverageConsole::TestBasic},
		    TestCoverageConsole::GetOutputBinaryPath().wstring(),
		    L"TestCoverageConsole"};

		auto mergedCoverage = TestTools::ComputeCoverageDataPatterns(args);
		args.lineMerging_ = false;
		auto perLineCoverage = TestTools::ComputeCoverageDataPatterns(args);

		TestHelper::CoverageDataComparer().AssertEquals(mergedCoverage,
		                                                perLineCoverage);
	}
}
