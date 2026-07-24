// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2014 OpenCppCoverage

#pragma once

#include <Windows.h>

#include <string>
#include <functional>
#include <filesystem>

#include "talea-cpp-cov/OptionsParser.hpp"
#include "talea-cpp-cov/Options.hpp"
#include "talea-cpp-cov/SubstitutePdbSourcePath.hpp"

namespace Coverage
{
	class CoverageData;
}

namespace TaleaCppCovTest
{
	namespace TestTools
	{
		using T_HandlesFct = std::function<void(HANDLE hProcess, HANDLE hFile)>;

		void GetHandles(const std::filesystem::path&, T_HandlesFct);

		std::optional<TaleaCppCov::Options> Parse(
			const TaleaCppCov::OptionsParser& parser,
			const std::vector<std::string>& arguments,
			bool appendProgramToRun = true,
			std::wostream* emptyOptionsExplanation = nullptr);

		const std::string GetOptionPrefix();
		const std::string GetProgramToRun();

		//---------------------------------------------------------------------
		struct CoverageArgs
		{
			CoverageArgs(
				const std::vector<std::wstring>& arguments,
				const std::wstring& modulePattern,
				const std::wstring& sourcePattern);

			std::filesystem::path programToRun_;
			std::vector<std::wstring> arguments_;
			std::vector<std::wstring> modulePatternCollection_;
			std::vector<std::wstring> sourcePatternCollection_;
			std::vector<TaleaCppCov::UnifiedDiffSettings> unifiedDiffSettingsCollection_;
			bool coverChildren_ = true;
			bool continueAfterCppException_ = false;
			bool optimizedBuildSupport_ = false;
			bool branchCoverage_ = false;
			bool lineMerging_ = true;
			bool preparseImports_ = false;
			std::vector<std::wstring> excludedLineRegexes_;
			std::vector<TaleaCppCov::SubstitutePdbSourcePath> substitutePdbSourcePath_;
		};

		//---------------------------------------------------------------------
		Coverage::CoverageData ComputeCoverageData(
			const std::vector<std::wstring>& arguments,
			const std::wstring& modulePattern,
			const std::wstring& sourcePattern);

		//---------------------------------------------------------------------
		Coverage::CoverageData ComputeCoverageDataPatterns(const CoverageArgs& args);

		//---------------------------------------------------------------------
		// Same settings as ComputeCoverageDataPatterns, run through the
		// experimental in-process VEH engine instead of the debugger.
		Coverage::CoverageData ComputeCoverageDataVeh(const CoverageArgs& args);
	}
}

