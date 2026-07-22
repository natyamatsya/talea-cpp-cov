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
#include "TestCoverageOptimizedBuild/TestCoverageOptimizedBuild.hpp"
#include "TestHelper/CoverageDataComparer.hpp"
#include "TestHelper/Tools.hpp"
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
		void AssertVehMatchesDebuggerArgs(const TestTools::CoverageArgs& args)
		{
			auto debuggerCoverage =
			    TestTools::ComputeCoverageDataPatterns(args);
			auto vehCoverage = TestTools::ComputeCoverageDataVeh(args);

			TestHelper::CoverageDataComparer().AssertEquals(debuggerCoverage,
			                                                vehCoverage);
		}

		//---------------------------------------------------------------------
		void AssertVehMatchesDebugger(const std::wstring& testArgument)
		{
			AssertVehMatchesDebuggerArgs(VehArgs(testArgument));
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
	// Child-process coverage (experimental): the helper hooks CreateProcess in
	// the target, force-suspends each child, and the host injects the helper
	// into it. The child's coverage (here the TestBasic path, which runs only
	// in the child) must match the debugger's --cover_children output.
	TEST(VehCoverageEngineTest, ChildProcessMatchesDebugger)
	{
		auto args = VehArgs(TestCoverageConsole::TestChildProcess);
		args.arguments_.push_back(TestCoverageConsole::TestBasic);
		args.coverChildren_ = true;

		auto debuggerCoverage = TestTools::ComputeCoverageDataPatterns(args);
		auto vehCoverage = TestTools::ComputeCoverageDataVeh(args);

		TestHelper::CoverageDataComparer().AssertEquals(debuggerCoverage,
		                                                vehCoverage);
	}

	//-------------------------------------------------------------------------
	// Recursive child coverage: the root spawns a child that itself spawns a
	// grandchild. The helper's CreateProcess hook propagates into each level,
	// so the grandchild (running TestBasic) is covered too - matching the
	// debugger's --cover_children across the whole tree.
	TEST(VehCoverageEngineTest, NestedChildProcessMatchesDebugger)
	{
		auto args = VehArgs(TestCoverageConsole::TestNestedChildProcess);
		args.coverChildren_ = true;

		auto debuggerCoverage = TestTools::ComputeCoverageDataPatterns(args);
		auto vehCoverage = TestTools::ComputeCoverageDataVeh(args);

		TestHelper::CoverageDataComparer().AssertEquals(debuggerCoverage,
		                                                vehCoverage);
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

		AssertVehMatchesDebuggerArgs(args);
	}

	//-------------------------------------------------------------------------
	// Optimized code: line records share addresses and functions are
	// reordered, exercising the VEH engine's line/address handling on the
	// optimized-build path.
	//
	// Known VEH limitation (see docs/PERFORMANCE_ROADMAP.md): it diverges from
	// the debugger on optimized code (e.g. over-reporting a line that shares an
	// address with an executed one). Reproduced with the CLI, so it is an
	// engine gap. Enable this test once VEH matches the debugger here.
	TEST(VehCoverageEngineTest, DISABLED_OptimizedBuildMatchesDebugger)
	{
		TestTools::CoverageArgs args{
		    {TestCoverageConsole::TestOptimizedBuild},
		    TestCoverageOptimizedBuild::GetOutputBinaryPath().wstring(),
		    TestCoverageOptimizedBuild::GetMainCppPath().filename().wstring()};
		args.coverChildren_ = false;
		args.optimizedBuildSupport_ = true;

		AssertVehMatchesDebuggerArgs(args);
	}

	//-------------------------------------------------------------------------
	// The same source file compiled into two modules: the VEH engine patches
	// both the executable and the DLL and must produce the debugger's
	// per-module coverage for each.
	TEST(VehCoverageEngineTest, MultipleModulesMatchesDebugger)
	{
		TestTools::CoverageArgs args{
		    {TestCoverageConsole::TestFileInSeveralModules},
		    TestCoverageSharedLib::GetOutputBinaryPath().wstring(),
		    TestCoverageSharedLib::GetSharedFunctionFilename().wstring()};
		args.modulePatternCollection_.push_back(
		    TestCoverageConsole::GetOutputBinaryPath().wstring());
		args.coverChildren_ = false;

		AssertVehMatchesDebuggerArgs(args);
	}

	//-------------------------------------------------------------------------
	// A DLL loaded, unloaded and loaded again: the VEH engine re-patches it on
	// the second load and covers it, matching the debugger. The unload
	// notification retires the module's active range and releases its claim so
	// the reload re-plants the probes into the same (cumulative) hit array.
	TEST(VehCoverageEngineTest, UnloadReloadDllMatchesDebugger)
	{
		TestTools::CoverageArgs args{
		    {TestCoverageConsole::TestUnloadReloadDll},
		    TestHelper::GetOutputBinaryPath().wstring(),
		    TestHelper::GetTestUnloadDllFilename().wstring()};
		args.coverChildren_ = false;

		AssertVehMatchesDebuggerArgs(args);
	}

	//-------------------------------------------------------------------------
	// An unhandled access violation terminates the target mid-run. The
	// injected VEH handler sits in the exception path, so coverage collected
	// up to the fault must still match the debugger's.
	TEST(VehCoverageEngineTest, UnhandledSehExceptionMatchesDebugger)
	{
		AssertVehMatchesDebugger(
		    TestCoverageConsole::TestThrowUnHandledSEHException);
	}
}
