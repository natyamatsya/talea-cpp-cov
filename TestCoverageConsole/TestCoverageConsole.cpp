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

#include <windows.h>
#include <tchar.h>

#include <string>
#include <iostream>

#include "TestCoverageSharedLib/TestCoverageSharedLib.hpp"
#include "TestCoverageOptimizedBuild/TestCoverageOptimizedBuild.hpp"

#include "SpecialLineInfo.hpp"
#include "TestCoverageConsole.hpp"
#include "TestBasic.hpp"
#include "TestBranch.hpp"
#include "TestThread.hpp"
#include "FileWithSpecialCharéàè.hpp"
#include "TestDiff.hpp"

namespace
{
	//-----------------------------------------------------------------------------
	void ThrowHandledException()
	{
		try
		{
			throw 42;
		}
		catch (...)
		{
		}
	}	

	//-----------------------------------------------------------------------------
	void TestFileInSeveralModules()
	{
		TestCoverageSharedLib::CallSharedFunctionFromSharedLib();
		TestCoverageSharedLib::SharedFunction(false);
	}
}

//-----------------------------------------------------------------------------
int _tmain(int argc, _TCHAR* argv[]) //! tmain-start
{
	if (argc < 2)
		std::wcout << L"Invalid number of argument:  " << argc << std::endl;
	else
	{
		std::wstring type = argv[1];

		if (type == TestCoverageConsole::TestBasic)
			TestCoverageConsole::RunTestBasic();
		else if (type == TestCoverageConsole::TestThread)
			TestCoverageConsole::RunThread();
		else if (type == TestCoverageConsole::TestSharedLib)
			TestCoverageSharedLib::IsOdd(42);
		else if (type == TestCoverageConsole::TestThrowHandledException)
			ThrowHandledException();
		else if (type == TestCoverageConsole::TestThrowUnHandledCppException)
			throw 42;
		else if (type == TestCoverageConsole::TestThrowUnHandledSEHException)
			*[] { static int* volatile p = nullptr; return p; }() = 42; // deliberate AV; volatile hides the null so clang keeps it
		else if (type == TestCoverageConsole::TestBreakPoint)
			DebugBreak();
		else if (type == TestCoverageConsole::TestChildProcess)
			TestCoverageConsole::RunChildProcesses(argc, argv);
		else if (type == TestCoverageConsole::TestNestedChildProcess)
			TestCoverageConsole::RunNestedChildProcess();
		else if (type == TestCoverageConsole::TestFileInSeveralModules)
			TestFileInSeveralModules();
		else if (type == TestCoverageConsole::TestSpecialLineInfo)
			TestCoverageConsole::SpecialLineInfo();
		else if (TestCoverageConsole::NeverTrue()) // keep the symbol; opaque so clang-cl /Od can't fold the branch away
			TestCoverageConsole::GetFileWithSpecialChars();
		else if (type == TestCoverageConsole::TestUnloadReloadDll)
			TestCoverageConsole::UnloadReloadDll();
		else if (type == TestCoverageConsole::TestDiff)
			TestCoverageConsole::FilterByDiff();
		else if (type == TestCoverageConsole::TestOptimizedBuild)
			TestCoverageOptimizedBuild::TestOptimizedBuild();
		else if (type == TestCoverageConsole::TestBranch)
			TestCoverageConsole::RunTestBranch(argc > 2 && std::wstring(argv[2]) == L"1");
		else
			std::wcerr << L"Unsupported type:" << type << std::endl;
	}
	return 0; //! tmain-return
}
