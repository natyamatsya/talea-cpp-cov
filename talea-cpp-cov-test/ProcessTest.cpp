// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2014 OpenCppCoverage

#include "stdafx.h"

#include "tools/Tool.hpp"

#include "talea-cpp-cov/StartInfo.hpp"
#include "talea-cpp-cov/Process.hpp"

#include "test-coverage-console/TestCoverageConsole.hpp"
#include "test-coverage-shared-lib/TestCoverageSharedLib.hpp"
#include "test-helper/Tools.hpp"

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