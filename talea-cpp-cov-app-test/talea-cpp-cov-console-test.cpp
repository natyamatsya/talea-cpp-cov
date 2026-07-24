// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2014 OpenCppCoverage

#include "stdafx.h"
#include <cstdint>

#include <Windows.h>
#include <filesystem>

#include "test-coverage-console/TestCoverageConsole.hpp"
#include "talea-cpp-cov-app/talea-cpp-cov.hpp"
#include "talea-cpp-cov/OptionsParser.hpp"
#include "talea-cpp-cov/ProgramOptions.hpp"
#include "talea-cpp-cov/FilterAssistant.hpp"
#include "talea-cpp-cov/ExportOptionParser.hpp"

#include "tools/ScopedAction.hpp"
#include "tools/Tool.hpp"
#include "test-helper/TemporaryPath.hpp"

#include "talea-cpp-cov-test-tools.hpp"

namespace fs = std::filesystem;
namespace cov = TaleaCppCov;

namespace TaleaCppCovAppTest
{	
	namespace
	{
		//---------------------------------------------------------------------
		void CheckOutputDirectory(const TestHelper::TemporaryPath& tempFolder)
		{			
			ASSERT_FALSE(fs::is_empty(tempFolder.GetPath()));
		}

		//---------------------------------------------------------------------
		std::int32_t RunCoverageForProgram(
		    const std::vector<std::pair<std::string, std::string>>&
		        additionalCoverageArguments,
		    const fs::path& programToRun,
		    const std::vector<std::wstring>& arguments)
		{
			auto coverageArguments = additionalCoverageArguments;
			AddDefaultFilters(coverageArguments, programToRun);
			coverageArguments.emplace_back(cov::ProgramOptions::QuietOption,
			                               "");
			std::string ignoreOutput;
			return RunCoverageFor(
			    coverageArguments, programToRun, arguments, &ignoreOutput);
		}

		//---------------------------------------------------------------------
		std::int32_t RunCoverageForProgram(const fs::path& programToRun,
		                          const std::vector<std::wstring>& arguments)
		{
			TestHelper::TemporaryPath tempFolder;

			std::int32_t exitCode = RunCoverageForProgram(
			{ BuildExportTypeString(cov::ExportOptionParser::ExportTypeHtmlValue,
									  tempFolder.GetPath()) },
			    programToRun,
			    arguments);

			CheckOutputDirectory(tempFolder);
			return exitCode;
		}

		//---------------------------------------------------------------------
		std::wstring
		GetOutput(const std::pair<std::string, std::string>& coverageArgument)
		{
			std::string output;
			auto exitCode =
			    RunCoverageFor({coverageArgument},
			                   TestCoverageConsole::GetOutputBinaryPath(),
			                   {},
			                   &output);
			if (exitCode != 0)
				throw std::runtime_error("Exit code invalid");
			return Tools::LocalToWString(output);
		}
	}

	//-------------------------------------------------------------------------
	TEST(TaleaCppCovAppConsoleTest, Basic)
	{		
		fs::path testCoverageConsole = TestCoverageConsole::GetOutputBinaryPath();
							
		ASSERT_EQ(0, RunCoverageForProgram(testCoverageConsole, {}));
	}	

	//-------------------------------------------------------------------------
	TEST(TaleaCppCovAppConsoleTest, UnhandledException)
	{
		fs::path testCoverageConsole = TestCoverageConsole::GetOutputBinaryPath();

		ASSERT_NE(0, RunCoverageForProgram(testCoverageConsole, { TestCoverageConsole::TestThrowUnHandledSEHException }));
	}
	
	//-------------------------------------------------------------------------
	TEST(TaleaCppCovAppConsoleTest, Breakpoint)
	{
		fs::path testCoverageConsole = TestCoverageConsole::GetOutputBinaryPath();

		ASSERT_EQ(EXCEPTION_BREAKPOINT, RunCoverageForProgram(testCoverageConsole, { TestCoverageConsole::TestBreakPoint }));
	}

	//-------------------------------------------------------------------------
	TEST(TaleaCppCovAppConsoleTest, BreakpointChildProcess)
	{
		fs::path testCoverageConsole = TestCoverageConsole::GetOutputBinaryPath();

		ASSERT_EQ(0, RunCoverageForProgram(testCoverageConsole,
			{ TestCoverageConsole::TestChildProcess, TestCoverageConsole::TestBreakPoint }));
	}

	//-------------------------------------------------------------------------
	TEST(TaleaCppCovAppConsoleTest, ExporterTest)
	{
		fs::path exporterTest{ OUT_DIR };
		
		exporterTest /= "ExporterTest.exe";
		ASSERT_EQ(0, RunCoverageForProgram(exporterTest, {}));
	}

	//-------------------------------------------------------------------------
	TEST(TaleaCppCovAppConsoleTest, ExitCode)
	{
		TestHelper::TemporaryPath tempFolder;

		const fs::path testCoverageConsole =
		    TestCoverageConsole::GetOutputBinaryPath();
		std::vector<std::pair<std::string, std::string>>
		    additionalCoverageArguments = {BuildExportTypeString(
		        cov::ExportOptionParser::ExportTypeBinaryValue,
		        tempFolder.GetPath())};

		std::int32_t exitCode = RunCoverageForProgram(
		    additionalCoverageArguments,
		    testCoverageConsole,
		    {TestCoverageConsole::TestThrowUnHandledCppException});
		ASSERT_NE(0, exitCode);
		ASSERT_TRUE(Tools::FileExists(tempFolder));

		additionalCoverageArguments.emplace_back(
		    cov::ProgramOptions::InputCoverageValue, tempFolder->string());
		exitCode = RunCoverageForProgram(
		    additionalCoverageArguments,
		    testCoverageConsole,
		    {TestCoverageConsole::TestBasic});
		ASSERT_EQ(0, exitCode);
	}

	//-------------------------------------------------------------------------
	TEST(TaleaCppCovAppConsoleTest, FilterAssistantNoModules)
	{
		ASSERT_TRUE(
		    GetOutput({cov::ProgramOptions::SelectedModulesOption, "No match"})
		        .contains(cov::FilterAssistant::NoModulesSelectedMsg));
	}

	//-------------------------------------------------------------------------
	TEST(TaleaCppCovAppConsoleTest, FilterAssistantNoSourceFiles)
	{
		ASSERT_TRUE(
		    GetOutput({cov::ProgramOptions::SelectedSourcesOption, "No match"})
		        .contains(cov::FilterAssistant::NoSourceFilesSelectedMsg));
	}
}