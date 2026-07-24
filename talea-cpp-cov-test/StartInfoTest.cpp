// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2014 OpenCppCoverage

#include "stdafx.h"

#include <filesystem>

#include "talea-cpp-cov/StartInfo.hpp"
#include "tools/ExceptionBase.hpp"

#include "test-coverage-console/TestCoverageConsole.hpp"

namespace cov = TaleaCppCov;
namespace fs = std::filesystem;

namespace TaleaCppCovTest
{
	namespace
	{
		const std::wstring validFilename = TestCoverageConsole::GetOutputBinaryPath().wstring(); 
	}

	//-------------------------------------------------------------------------
	TEST(StartInfoTest, Constructor)
	{
		ASSERT_NO_THROW(cov::StartInfo s{ validFilename };);
	}

	//-------------------------------------------------------------------------
	TEST(StartInfoTest, SetWorkingDirectoryNotExists)
	{
		cov::StartInfo s(validFilename);
		fs::path folder{ L"" };
		
		ASSERT_THROW(s.SetWorkingDirectory(folder), std::runtime_error);
	}

	//-------------------------------------------------------------------------
	TEST(StartInfoTest, SetWorkingDirectoryExists)
	{
		cov::StartInfo s(validFilename);
		fs::path folder{ L"." };

		ASSERT_NO_THROW(s.SetWorkingDirectory(folder));
	}

}