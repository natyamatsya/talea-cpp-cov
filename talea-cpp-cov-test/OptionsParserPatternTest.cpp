// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2014 OpenCppCoverage

#include "stdafx.h"

#include "talea-cpp-cov/OptionsParser.hpp"
#include "talea-cpp-cov/Options.hpp"
#include "talea-cpp-cov/ProgramOptions.hpp"

#include "talea-cpp-cov-test/TestTools.hpp"

#include "tools/Tool.hpp"

namespace cov = TaleaCppCov;

namespace TaleaCppCovTest
{
	namespace
	{
		//-------------------------------------------------------------------------
		std::optional<cov::Options> Parse(
			const std::string& optionName,
			const std::wstring& value)
		{
			cov::OptionsParser parser;
			std::vector<std::string> arguments = { TestTools::GetOptionPrefix() + optionName, 
													Tools::ToLocalString(value) };

			return TestTools::Parse(parser, arguments);
		}

		//-------------------------------------------------------------------------
		void CheckPatternOption(
			const std::string& optionName,
			const std::wstring& value,
			std::function<std::wstring(const cov::Options&)> getOption)
		{
			auto options = Parse(optionName, value);
			auto option = getOption(*options);

			ASSERT_EQ(value, option);
		}
	}

	//-------------------------------------------------------------------------
	TEST(OptionsParserPatternTest, SelectedModulePatterns)
	{
		CheckPatternOption(cov::ProgramOptions::SelectedModulesOption, L"module",
			[](const cov::Options& options) { return options.GetModulePatterns().GetSelectedPatterns().front(); }
		);
	}

	//-------------------------------------------------------------------------
	TEST(OptionsParserPatternTest, ExcludedModulePatterns)
	{
		CheckPatternOption(cov::ProgramOptions::ExcludedModulesOption, L"module",
			[](const cov::Options& options) { return options.GetModulePatterns().GetExcludedPatterns().front(); }
		);
	}

	//-------------------------------------------------------------------------
	TEST(OptionsParserPatternTest, SelectedSourcePatterns)
	{
		CheckPatternOption(cov::ProgramOptions::SelectedSourcesOption, L"source",
			[](const cov::Options& options) { return options.GetSourcePatterns().GetSelectedPatterns().front(); }
		);
	}

	//-------------------------------------------------------------------------
	TEST(OptionsParserPatternTest, ExcludedSourcePatterns)
	{
		CheckPatternOption(cov::ProgramOptions::ExcludedSourcesOption, L"source",
			[](const cov::Options& options) { return options.GetSourcePatterns().GetExcludedPatterns().front(); }
		);
	}

	//-------------------------------------------------------------------------
	TEST(OptionsParserPatternTest, CheckPattern)
	{
		for (const auto& optionName: {
		         cov::ProgramOptions::SelectedSourcesOption,
				 cov::ProgramOptions::ExcludedSourcesOption,
				 cov::ProgramOptions::SelectedModulesOption,
				 cov::ProgramOptions::ExcludedModulesOption })
		{
			for (const auto& value : {L".", L"..", L"\\.", L"\\.\\"})
				ASSERT_EQ(std::nullopt, Parse(optionName, value));
			for (const auto& value : {L"test.txt", L"test\\test.txt"})
				ASSERT_NE(std::nullopt, Parse(optionName, value));

			ASSERT_EQ(std::nullopt, Parse(optionName, L"SubFolder//Folder"));
		}
	}
}