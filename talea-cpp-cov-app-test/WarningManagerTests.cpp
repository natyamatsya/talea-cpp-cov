// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2018 OpenCppCoverage

#include "stdafx.h"
#include <cstdint>

#include "talea-cpp-cov-test-tools.hpp"
#include "test-coverage-console/TestCoverageConsole.hpp"
#include "talea-cpp-cov/ProgramOptions.hpp"
#include "talea-cpp-cov/OptionsParser.hpp"
#include "tools/Tool.hpp"

using TaleaCppCov::OptionsParser;
using TaleaCppCov::ProgramOptions;

namespace TaleaCppCovAppTest
{
	//-------------------------------------------------------------------------
	TEST(WarningManagerTests, CommandLineTooLong)
	{
		std::string stdOut;
		const std::vector<std::pair<std::string, std::string>>
		    coverageArguments = {
		        {ProgramOptions::SelectedSourcesOption,
		         TestCoverageConsole::GetMainCppPath().string()},
		        {ProgramOptions::SelectedModulesOption,
		         TestCoverageConsole::GetOutputBinaryPath().string()},
		        {ProgramOptions::QuietOption, ""}};
		std::vector<std::wstring> arguments{TestCoverageConsole::TestBasic};

		RunCoverageFor(coverageArguments,
		               TestCoverageConsole::GetOutputBinaryPath(),
		               arguments,
		               &stdOut);
		ASSERT_EQ("", stdOut);
		for (std::int32_t i = 0; i < OptionsParser::DosCommandLineMaxSize / 10; ++i)
			arguments.push_back(L"01223456789");

		RunCoverageFor(coverageArguments,
		               TestCoverageConsole::GetOutputBinaryPath(),
		               arguments,
		               &stdOut);
		auto output = Tools::LocalToWString(stdOut);
		ASSERT_TRUE(
		    output.contains(OptionsParser::GetTooLongCommandLineMessage()));
	}
}
