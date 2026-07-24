// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2014 OpenCppCoverage

#include "stdafx.h"
#include <cstdint>
#include "talea-cpp-cov-test-tools.hpp"

#include <filesystem>

#include "test-helper/Tools.hpp"
#include "tools/Tool.hpp"
#include "talea-cpp-cov/OptionsParser.hpp"
#include "talea-cpp-cov/ProgramOptions.hpp"
#include "talea-cpp-cov/ExportOptionParser.hpp"

namespace cov = TaleaCppCov;

namespace TaleaCppCovAppTest
{
	//---------------------------------------------------------------------
	void AddDefaultFilters(
		std::vector<std::pair<std::string, std::string>>& coverageArguments,	
		const std::filesystem::path& programToRun)
	{
		coverageArguments.push_back({ cov::ProgramOptions::SelectedModulesOption, programToRun.string() });
		coverageArguments.push_back({ cov::ProgramOptions::SelectedSourcesOption, GetSolutionFolderName() });
	}

	//-------------------------------------------------------------------------
	void AddDefaultHtmlExport(
		std::vector<std::pair<std::string, std::string>>& coverageArguments,
		const std::filesystem::path& outputFolder)
	{
		coverageArguments.push_back(
		{ cov::ExportOptionParser::ExportTypeOption, cov::ExportOptionParser::ExportTypeHtmlValue
		+ cov::ExportOptionParser::ExportSeparator + outputFolder.string() });		
	}

	//---------------------------------------------------------------------
	std::pair<std::string, std::string> BuildExportTypeString(
		const std::string& exportType,
		const std::filesystem::path& output)
	{
		return{
			cov::ExportOptionParser::ExportTypeOption,
			exportType + cov::ExportOptionParser::ExportSeparator + output.string() };
	}

	//-------------------------------------------------------------------------
	std::int32_t RunCoverageFor(
		const std::vector<std::pair<std::string, std::string>>& coverageArguments,
		const std::filesystem::path& programToRun,
		const std::vector<std::wstring>& arguments,
		std::string* optionalOutput)
	{
		std::vector<std::string> allCoverageArguments;

		for (const auto& keyValue : coverageArguments)
		{
			allCoverageArguments.push_back("--" + keyValue.first);
			const auto& value = keyValue.second;

			if (!value.empty())
				allCoverageArguments.push_back(value);
		}

		allCoverageArguments.push_back(programToRun.string());
		for (const auto& argument : arguments)
			allCoverageArguments.push_back(Tools::ToLocalString(argument));

		std::filesystem::path taleaCppCovApp(OUT_DIR);
		taleaCppCovApp /= "talea-cpp-cov.exe";

		auto result = TestHelper::LaunchProcess(taleaCppCovApp,
		                                        allCoverageArguments,
		                                        ".",
		                                        optionalOutput != nullptr);
		if (optionalOutput)
			*optionalOutput = result.output;

		return result.exitCode;
	}

	//-------------------------------------------------------------------------
	std::string GetSolutionFolderName()
	{
		return std::filesystem::canonical(SOLUTION_DIR).filename().string();
	}
}