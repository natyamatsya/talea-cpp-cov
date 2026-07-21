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

#include "TestTools.hpp"
#include "TestCoverageConsole/TestCoverageConsole.hpp"
#include "TestCoverageSharedLib/TestCoverageSharedLib.hpp"
#include "TestHelper/CoverageDataComparer.hpp"
#include "Plugin/Exporter/CoverageData.hpp"

namespace CppCoverageTest
{
	namespace
	{
		//---------------------------------------------------------------------
		TestTools::CoverageArgs VehArgs(const std::wstring& testArgument)
		{
			TestTools::CoverageArgs args{
			    {testArgument},
			    TestCoverageConsole::GetOutputBinaryPath().wstring(),
			    L"TestCoverageConsole"};
			// The VEH engine instruments only the main executable.
			args.modulePatternCollection_ = {
			    TestCoverageConsole::GetOutputBinaryPath().wstring()};
			args.coverChildren_ = false;
			return args;
		}

		//---------------------------------------------------------------------
		// The VEH engine must produce exactly the debugger's coverage: it is
		// a different collection mechanism, not a different measurement.
		void AssertVehMatchesDebugger(const std::wstring& testArgument)
		{
			auto args = VehArgs(testArgument);
			auto debuggerCoverage =
			    TestTools::ComputeCoverageDataPatterns(args);
			auto vehCoverage = TestTools::ComputeCoverageDataVeh(args);

			TestHelper::CoverageDataComparer().AssertEquals(debuggerCoverage,
			                                                vehCoverage);
		}
	}

	//-------------------------------------------------------------------------
	TEST(VehCoverageEngineTest, BasicMatchesDebugger)
	{
		AssertVehMatchesDebugger(TestCoverageConsole::TestBasic);
	}

	//-------------------------------------------------------------------------
	TEST(VehCoverageEngineTest, ThreadMatchesDebugger)
	{
		AssertVehMatchesDebugger(TestCoverageConsole::TestThread);
	}

	//-------------------------------------------------------------------------
	TEST(VehCoverageEngineTest, HandledExceptionMatchesDebugger)
	{
		AssertVehMatchesDebugger(
		    TestCoverageConsole::TestThrowHandledException);
	}

	//-------------------------------------------------------------------------
	TEST(VehCoverageEngineTest, MatchesDebuggerWithoutLineMerging)
	{
		auto args = VehArgs(TestCoverageConsole::TestBasic);
		args.lineMerging_ = false;

		auto debuggerCoverage = TestTools::ComputeCoverageDataPatterns(args);
		auto vehCoverage = TestTools::ComputeCoverageDataVeh(args);

		TestHelper::CoverageDataComparer().AssertEquals(debuggerCoverage,
		                                                vehCoverage);
	}

	//-------------------------------------------------------------------------
	// Branch coverage: the VEH engine discovers the same Jcc offline and, via
	// its in-process trap-flag single-step re-arm, records the same outcome the
	// debugger does through the debug loop.
	TEST(VehCoverageEngineTest, BranchMatchesDebugger)
	{
		// Both the taken (1) and the fall-through (0) direction, so the
		// in-process condition evaluation is checked for each outcome.
		for (const auto* conditionValue : {L"1", L"0"})
		{
			auto args = VehArgs(TestCoverageConsole::TestBranch);
			args.arguments_.push_back(conditionValue);
			args.branchCoverage_ = true;

			auto debuggerCoverage =
			    TestTools::ComputeCoverageDataPatterns(args);
			auto vehCoverage = TestTools::ComputeCoverageDataVeh(args);

			TestHelper::CoverageDataComparer().AssertEquals(debuggerCoverage,
			                                                vehCoverage);
		}
	}

	//-------------------------------------------------------------------------
	// A DLL is loaded and patched through the DLL-load notification, not the
	// main module: the VEH engine must cover it exactly like the debugger.
	TEST(VehCoverageEngineTest, DllMatchesDebugger)
	{
		TestTools::CoverageArgs args{
		    {TestCoverageConsole::TestSharedLib},
		    TestCoverageSharedLib::GetOutputBinaryPath().wstring(),
		    TestCoverageSharedLib::GetMainCppPath().wstring()};
		args.coverChildren_ = false;

		auto debuggerCoverage = TestTools::ComputeCoverageDataPatterns(args);
		auto vehCoverage = TestTools::ComputeCoverageDataVeh(args);

		TestHelper::CoverageDataComparer().AssertEquals(debuggerCoverage,
		                                                vehCoverage);
	}
}
