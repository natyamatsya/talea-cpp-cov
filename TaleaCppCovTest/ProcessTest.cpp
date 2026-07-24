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

#include "stdafx.h"

#include "Tools/Tool.hpp"

#include "TaleaCppCov/StartInfo.hpp"
#include "TaleaCppCov/Process.hpp"

#include "TestCoverageConsole/TestCoverageConsole.hpp"
#include "TestCoverageSharedLib/TestCoverageSharedLib.hpp"
#include "TestHelper/Tools.hpp"

namespace cov = TaleaCppCov;

namespace TaleaCppCovTest
{
	//-------------------------------------------------------------------------
	TEST(Process, StartWithArgument)
	{
		cov::StartInfo startInfo{ TestCoverageConsole::GetOutputBinaryPath() };
		startInfo.AddArgument(TestCoverageConsole::TestThrowHandledException);
		
		cov::Process process{ startInfo };
		ASSERT_TRUE(process.Start(0));
	}

	//-------------------------------------------------------------------------
	TEST(Process, InvalidProgram)
	{
		cov::StartInfo startInfo{TestCoverageSharedLib::GetOutputBinaryPath()};

		cov::Process process{startInfo};
		auto result = process.Start(0);
		ASSERT_FALSE(result);
		ASSERT_NE(std::wstring::npos,
		          result.error().message.find(
		              cov::Process::CheckIfValidExecutableMessage));
	}

	//-------------------------------------------------------------------------
	TEST(Process, FileNotExists)
	{
		cov::StartInfo startInfo{"Does not exist."};

		cov::Process process{startInfo};
		auto result = process.Start(0);
		ASSERT_FALSE(result);
		ASSERT_NE(std::wstring::npos,
		          result.error().message.find(
		              cov::Process::CannotFindPathMessage));
	}

	//-------------------------------------------------------------------------
	TEST(Process, ProgramInPath)
	{
		cov::StartInfo startInfo{ L"cmd.exe" };
		startInfo.AddArgument(L"/C");

		cov::Process process{ startInfo };
		ASSERT_TRUE(process.Start(0));
	}
}