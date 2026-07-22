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

#include <string>
#include <filesystem>

namespace TestCoverageConsole
{
	//-------------------------------------------------------------------------
	inline std::filesystem::path GetMainCppPath()
	{
		// clang-cl's __FILE__ uses forward slashes; normalize to the native
		// separator so paths built from it satisfy OpenCppCoverage's
		// backslash-only --sources validation.
		return std::filesystem::path(__FILE__)
		    .replace_extension("cpp")
		    .make_preferred();
	}

	//-------------------------------------------------------------------------
	inline std::filesystem::path GetMainCppFilename()
	{
		return GetMainCppPath().filename();
	}

	//-------------------------------------------------------------------------
	inline std::filesystem::path GetOutputBinaryPath()
	{
		return std::filesystem::path(OUT_DIR) / "TestCoverageConsole.exe";
	}

	//-------------------------------------------------------------------------
	inline int GetTestCoverageConsoleCppMainStartLine()
	{
		return 57;
	}

	//-------------------------------------------------------------------------
	inline int GetTestCoverageConsoleCppMainReturnLine()
	{
		return GetTestCoverageConsoleCppMainStartLine() + 43;
	}

	const std::wstring TestBasic = L"TestBasic";
	const std::wstring TestBranch = L"TestBranch";
	const std::wstring TestThread = L"TestThread";
	const std::wstring TestSharedLib = L"TestSharedLib";
	const std::wstring TestThrowHandledException = L"TestThrowHandledException";
	const std::wstring TestThrowUnHandledCppException = L"TestThrowUnHandledCppException";
	const std::wstring TestThrowUnHandledSEHException = L"TestThrowUnHandledSEHException";
	const std::wstring TestBreakPoint = L"TestBreakPoint";
	const std::wstring TestChildProcess = L"ChildProcess";
	const std::wstring TestNestedChildProcess = L"NestedChildProcess";
	const std::wstring TestFileInSeveralModules = L"FileInSeveralModules";
	const std::wstring TestSpecialLineInfo = L"TestSpecialLineInfo";
	const std::wstring TestUnloadReloadDll = L"TestUnloadReloadDll";
	const std::wstring TestDiff = L"TestDiff";
	const std::wstring TestOptimizedBuild = L"TestOptimizedBuild";
}
