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
#include "TestTools.hpp"

#include <filesystem>
#include "tools/StringUtils.hpp"

#include "talea-cpp-cov/StartInfo.hpp"
#include "talea-cpp-cov/Debugger.hpp"
#include "talea-cpp-cov/IDebugEventsHandler.hpp"
#include "talea-cpp-cov/CodeCoverageRunner.hpp"
#include "talea-cpp-cov/VehCoverageEngine.hpp"
#include "talea-cpp-cov/CoverageFilterSettings.hpp"
#include "talea-cpp-cov/RunCoverageSettings.hpp"
#include "talea-cpp-cov/Patterns.hpp"

#include "test-coverage-console/TestCoverageConsole.hpp"

#include "tools/WarningManager.hpp"

namespace cov = TaleaCppCov;

namespace TaleaCppCovTest
{
	namespace
	{
		struct DebugEventsHandler : public cov::IDebugEventsHandler
		{
			explicit DebugEventsHandler(TestTools::T_HandlesFct action)
			: action_(action)
			{
			}

			virtual void OnCreateProcess(const CREATE_PROCESS_DEBUG_INFO& processInfo) override
			{
				action_(processInfo.hProcess, processInfo.hFile);
			}

		private:
			TestTools::T_HandlesFct action_;
		};

	}

	namespace TestTools
	{
		//-------------------------------------------------------------------------
		void GetHandles(const std::filesystem::path& path, TestTools::T_HandlesFct action)
		{
			cov::StartInfo startInfo{ path };
			cov::Debugger debugger{ false, false, false };
			DebugEventsHandler debugEventsHandler{ action };

			[[maybe_unused]] auto exitCode =
			    Tools::ValueOrThrow(debugger.Debug(startInfo, debugEventsHandler));
		}

		//---------------------------------------------------------------------
		std::optional<cov::Options> Parse(
			const cov::OptionsParser& parser,
			const std::vector<std::string>& arguments,
			bool appendProgramToRun,
			std::wostream* emptyOptionsExplanation)
		{
			std::vector<const char*> argv;

			argv.push_back("programName");
			for (const auto& argument : arguments)
				argv.push_back(argument.c_str());

			auto programToRun = GetProgramToRun();
			if (appendProgramToRun)
				argv.push_back(programToRun.c_str());
			return parser.Parse(static_cast<int>(argv.size()), &argv[0], emptyOptionsExplanation);
		}

		const std::string GetOptionPrefix() { return "--"; }
		const std::string GetProgramToRun() { return TestCoverageConsole::GetOutputBinaryPath().string(); }

		//---------------------------------------------------------------------
		CoverageArgs::CoverageArgs(
			const std::vector<std::wstring>& arguments,
			const std::wstring& modulePattern,
			const std::wstring& sourcePattern)
			: programToRun_{ TestCoverageConsole::GetOutputBinaryPath() },
			arguments_{ arguments },
			modulePatternCollection_{ modulePattern },
			sourcePatternCollection_{ sourcePattern }
		{
		}

		//---------------------------------------------------------------------
		Coverage::CoverageData ComputeCoverageData(
			const std::vector<std::wstring>& arguments,
			const std::wstring& modulePattern,
			const std::wstring& sourcePattern)
		{
			CoverageArgs args{ arguments, modulePattern, sourcePattern };

			return ComputeCoverageDataPatterns(args);
		}

		//---------------------------------------------------------------------
		Coverage::CoverageData ComputeCoverageDataPatterns(
			const CoverageArgs& args)
		{
			cov::CodeCoverageRunner codeCoverageRunner{
			    std::make_shared<Tools::WarningManager>()};
			cov::Patterns modulePatterns{false};
			cov::Patterns sourcePatterns{ false };

			for (auto modulePattern : args.modulePatternCollection_)
			{
				Tools::ToLower(modulePattern);
				modulePatterns.AddSelectedPatterns(modulePattern);
			}

			for (auto sourcePattern : args.sourcePatternCollection_)
			{
				Tools::ToLower(sourcePattern);
				sourcePatterns.AddSelectedPatterns(sourcePattern);
			}

			cov::CoverageFilterSettings coverageFilterSettings{ modulePatterns, sourcePatterns };
			cov::StartInfo startInfo{ args.programToRun_ };

			for (const auto& argument : args.arguments_)
				startInfo.AddArgument(argument);

			cov::RunCoverageSettings settings(
				startInfo,
				coverageFilterSettings,
				args.unifiedDiffSettingsCollection_,
				args.excludedLineRegexes_,
				args.substitutePdbSourcePath_);
			settings.SetCoverChildren(args.coverChildren_);
			settings.SetContinueAfterCppException(args.continueAfterCppException_);
			settings.SetOptimizedBuildSupport(args.optimizedBuildSupport_);
			settings.SetBranchCoverage(args.branchCoverage_);
			settings.SetLineMergingEnabled(args.lineMerging_);
			settings.SetPreparseImports(args.preparseImports_);

			auto coverageData = codeCoverageRunner.RunCoverage(settings);

			return coverageData;
		}

		//---------------------------------------------------------------------
		Coverage::CoverageData ComputeCoverageDataVeh(const CoverageArgs& args)
		{
			cov::VehCoverageEngine vehEngine{
			    std::make_shared<Tools::WarningManager>()};
			cov::Patterns modulePatterns{false};
			cov::Patterns sourcePatterns{false};

			for (auto modulePattern : args.modulePatternCollection_)
			{
				Tools::ToLower(modulePattern);
				modulePatterns.AddSelectedPatterns(modulePattern);
			}

			for (auto sourcePattern : args.sourcePatternCollection_)
			{
				Tools::ToLower(sourcePattern);
				sourcePatterns.AddSelectedPatterns(sourcePattern);
			}

			cov::CoverageFilterSettings coverageFilterSettings{
			    modulePatterns, sourcePatterns};
			cov::StartInfo startInfo{args.programToRun_};

			for (const auto& argument : args.arguments_)
				startInfo.AddArgument(argument);

			cov::RunCoverageSettings settings(
			    startInfo,
			    coverageFilterSettings,
			    args.unifiedDiffSettingsCollection_,
			    args.excludedLineRegexes_,
			    args.substitutePdbSourcePath_);
			// Keep this in sync with ComputeCoverageDataPatterns so a VEH-vs-
			// debugger comparison differs only by collection mechanism.
			settings.SetCoverChildren(args.coverChildren_);
			settings.SetContinueAfterCppException(args.continueAfterCppException_);
			settings.SetOptimizedBuildSupport(args.optimizedBuildSupport_);
			settings.SetBranchCoverage(args.branchCoverage_);
			settings.SetLineMergingEnabled(args.lineMerging_);
			settings.SetPreparseImports(args.preparseImports_);

			return Tools::ValueOrThrow(vehEngine.RunCoverage(settings));
		}
	}
}

