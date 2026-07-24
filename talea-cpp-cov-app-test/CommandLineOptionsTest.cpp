// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2014 OpenCppCoverage

#include "stdafx.h"
#include <cstdint>

#include <filesystem>
#include "tools/StringUtils.hpp"
#include "test-helper/TemporaryPath.hpp"
#include "talea-cpp-cov/OptionsParser.hpp"
#include "talea-cpp-cov/ProgramOptions.hpp"
#include "talea-cpp-cov-app/talea-cpp-cov.hpp"
#include "test-coverage-console/TestCoverageConsole.hpp"
#include "test-coverage-shared-lib/TestCoverageSharedLib.hpp"

#include "talea-cpp-cov-test-tools.hpp"

namespace fs = std::filesystem;
namespace cov = TaleaCppCov;

namespace TaleaCppCovAppTest
{
	namespace
	{
		auto testCoverageConsole = TestCoverageConsole::GetOutputBinaryPath();
		auto testCoverageSharedLib = TestCoverageSharedLib::GetOutputBinaryPath();
		
		auto testCoverageSharedLibMain = TestCoverageSharedLib::GetMainCppPath();

		//---------------------------------------------------------------------
		bool FindFilename(const std::wstring& filename, const fs::path& outputDirectory)
		{
			for (fs::recursive_directory_iterator it(outputDirectory);
				it != fs::recursive_directory_iterator(); ++it)
			{
				const auto& path = it->path();

				if (Tools::IEquals(path.filename().stem().wstring(), std::wstring_view{filename}))
					return true;
			}

			return false;
		}
		
		//---------------------------------------------------------------------
		class CommandLineOptionsTest : public ::testing::Test
		{
		public:
			//-----------------------------------------------------------------
			std::int32_t RunCoverageOnProgramWithExitCode(
				std::vector<std::pair<std::string, std::string>> coverageArguments,
				bool useSourceInSolutionDir = true)
			{
				if (useSourceInSolutionDir)
					coverageArguments.push_back({ cov::ProgramOptions::SelectedSourcesOption, GetSolutionFolderName()});
				auto thirdParties = { "Build", "packages" };
				for (const auto& thirdParty: thirdParties)
					coverageArguments.push_back({ cov::ProgramOptions::ExcludedSourcesOption, thirdParty });

				AddDefaultHtmlExport(coverageArguments, GetTempPath());
				coverageArguments.emplace_back(cov::ProgramOptions::QuietOption, "");
				std::string ignoreOutput;
				return RunCoverageFor(coverageArguments, testCoverageConsole, {}, &ignoreOutput);
			}

			//-----------------------------------------------------------------
			void RunCoverageOnProgram(
				std::vector<std::pair<std::string, std::string>> coverageArguments,
				bool useSourceInSolutionDir = true)
			{
				std::int32_t exitCode = RunCoverageOnProgramWithExitCode(coverageArguments,
					useSourceInSolutionDir);

				ASSERT_EQ(0, exitCode);
			}

			//-----------------------------------------------------------------
			void CheckFilenameExistsInOutput(const fs::path& path, bool expectedValue)
			{
				auto filename = path.filename().replace_extension("");
				CheckFilenameWithExtensionExistsInOutput(filename, expectedValue);
			}

			//-----------------------------------------------------------------
			void CheckFilenameWithExtensionExistsInOutput(const fs::path& path, bool expectedValue)
			{
				auto filename = path.filename();
				ASSERT_EQ(expectedValue, FindFilename(filename.wstring(), GetTempPath()));
			}

			//-----------------------------------------------------------------
			const fs::path& GetTempPath() const
			{
				return tempFolder_.GetPath();
			}

		private:
			TestHelper::TemporaryPath tempFolder_;
		};
	}
	
	//-------------------------------------------------------------------------
	TEST_F(CommandLineOptionsTest, SelectedModulesOption)
	{		
		RunCoverageOnProgram({ { cov::ProgramOptions::SelectedModulesOption, testCoverageConsole.string() } });
		CheckFilenameExistsInOutput(testCoverageConsole, true);
		CheckFilenameExistsInOutput(testCoverageSharedLib, false);
	}

	//-------------------------------------------------------------------------
	TEST_F(CommandLineOptionsTest, ExcludedModulesOption)
	{
		RunCoverageOnProgram({ { cov::ProgramOptions::ExcludedModulesOption, testCoverageConsole.string() } });
		CheckFilenameExistsInOutput(testCoverageConsole, false);
		CheckFilenameExistsInOutput(testCoverageSharedLib, true);
	}

	//-------------------------------------------------------------------------
	TEST_F(CommandLineOptionsTest, SelectedSourcesOption)
	{
		auto testCoverageConsoleMain = TestCoverageConsole::GetMainCppFilename().string();

		RunCoverageOnProgram({ { cov::ProgramOptions::SelectedSourcesOption, testCoverageConsoleMain } }, false);
		CheckFilenameWithExtensionExistsInOutput(testCoverageConsoleMain, true);
		CheckFilenameWithExtensionExistsInOutput(testCoverageSharedLibMain, false);
	}

	//-------------------------------------------------------------------------
	TEST_F(CommandLineOptionsTest, ExcludedSourcesOption)
	{
		auto testCoverageConsoleMain = TestCoverageConsole::GetMainCppFilename().string();

		RunCoverageOnProgram({ { cov::ProgramOptions::ExcludedSourcesOption, testCoverageConsoleMain } });
		CheckFilenameWithExtensionExistsInOutput(testCoverageConsoleMain, false);
		CheckFilenameWithExtensionExistsInOutput(testCoverageSharedLibMain, true);
	}

	//-------------------------------------------------------------------------
	TEST_F(CommandLineOptionsTest, OutputDirectoryOption)
	{
		RunCoverageOnProgram({});
		CheckFilenameExistsInOutput(testCoverageConsole, true);
		CheckFilenameExistsInOutput(testCoverageSharedLib, true);
	}

	//-------------------------------------------------------------------------
	TEST_F(CommandLineOptionsTest, FailureExitCode)
	{
		std::vector<std::pair<std::string, std::string>> coverageArguments;

		ASSERT_EQ(0, RunCoverageOnProgramWithExitCode(coverageArguments));

		coverageArguments.emplace_back("Invalid option", "Invalid value");
		ASSERT_EQ(TaleaCppCovApp::FailureExitCode,
		          RunCoverageOnProgramWithExitCode(coverageArguments));

		// This should failed at runtime
		coverageArguments.clear();
		coverageArguments.emplace_back(cov::ProgramOptions::InputCoverageValue,
		                               __FILE__);
		ASSERT_EQ(TaleaCppCovApp::FailureExitCode,
		          RunCoverageOnProgramWithExitCode(coverageArguments));
	}
}
