// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2014 OpenCppCoverage

#pragma once
#include <cstdint>

#include <vector>
#include <string>
#include <filesystem>

namespace TaleaCppCovAppTest
{	
	void AddDefaultFilters(
		std::vector<std::pair<std::string, std::string>>& coverageArguments,
		const std::filesystem::path& programToRun);

	void AddDefaultHtmlExport(
		std::vector<std::pair<std::string, std::string>>& coverageArguments,
		const std::filesystem::path& outputFolder);
	
	std::pair<std::string, std::string> BuildExportTypeString(
		const std::string& exportType,
		const std::filesystem::path& output);
	
	std::int32_t RunCoverageFor(
		const std::vector<std::pair<std::string, std::string>>& coverageArguments,
		const std::filesystem::path& programToRun,
		const std::vector<std::wstring>& arguments,
		std::string* optionalOutput = nullptr);

	std::string GetSolutionFolderName();
}