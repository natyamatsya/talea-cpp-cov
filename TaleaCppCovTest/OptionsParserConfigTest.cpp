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

#include <fstream>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "TaleaCppCov/ProgramOptions.hpp"
#include "TaleaCppCov/Options.hpp"
#include "TaleaCppCov/OptionsParser.hpp"
#include "TaleaCppCov/OptionsSpec.hpp"
#include "TaleaCppCov/GlazeCli.hpp"

#include "TaleaCppCovTest/TestTools.hpp"

#include "TestHelper/TemporaryPath.hpp"
#include "Tools/Tool.hpp"

namespace cov = TaleaCppCov;

namespace TaleaCppCovTest
{
	namespace
	{
		//---------------------------------------------------------------------
		// Writes the config file as JSON (the format since the glaze port),
		// built from (option, value) pairs by parsing them into an OptionsSpec
		// and serializing it, then parses it with the given command line on top.
		std::optional<cov::Options> MutipleSourceParse(
			const std::vector<std::pair<std::string, std::wstring>>& configArguments,
			const std::vector<std::pair<std::string, std::wstring>>& commandLineArguments)
		{
			cov::OptionsParser parser;
			TestHelper::TemporaryPath path;

			cov::OptionsSpec configSpec;
			std::vector<std::string> configTokens;
			for (const auto& argument : configArguments)
			{
				configTokens.push_back("--" + argument.first);
				configTokens.push_back(Tools::ToLocalString(argument.second));
			}
			glzcli::parse(configSpec,
			              std::span<const std::string>{configTokens});

			std::string json;
			[[maybe_unused]] auto writeError =
			    glz::write_json(configSpec, json);
			{
				std::ofstream ofs(path.GetPath().string(), std::ios::binary);
				ofs << json;
			}

			std::vector<std::string> arguments{
				"--" + cov::ProgramOptions::ConfigFileOption,
				path.GetPath().string()};

			for (const auto& argumentValue : commandLineArguments)
			{
				arguments.push_back(TestTools::GetOptionPrefix() +
				                    argumentValue.first);
				arguments.push_back(Tools::ToLocalString(argumentValue.second));
			}

			return TestTools::Parse(parser, arguments);
		}
	}

	//-------------------------------------------------------------------------
	TEST(OptionsParserConfigTest, ConfigurationFileMultipleValue)
	{
		const std::wstring source1 = L"s1";
		const std::wstring source2 = L"s2";

		auto options = MutipleSourceParse(
		{ { cov::ProgramOptions::SelectedSourcesOption, source1 }, { cov::ProgramOptions::SelectedSourcesOption, source2 } },
		{});
		ASSERT_TRUE(static_cast<bool>(options));
		const auto& sourcePatterns = options->GetSourcePatterns().GetSelectedPatterns();
		ASSERT_EQ(2, sourcePatterns.size());
		ASSERT_EQ(source1, sourcePatterns[0]);
		ASSERT_EQ(source2, sourcePatterns[1]);
	}

	//-------------------------------------------------------------------------
	TEST(OptionsParserConfigTest, ConfigurationFileCmdLineOverride)
	{
		TestHelper::TemporaryPath directory1{ TestHelper::TemporaryPathOption::CreateAsFolder };
		TestHelper::TemporaryPath directory2{ TestHelper::TemporaryPathOption::CreateAsFolder };

		auto options = MutipleSourceParse(
		{ { cov::ProgramOptions::WorkingDirectoryOption, directory1.GetPath().wstring() } },
		{ { cov::ProgramOptions::WorkingDirectoryOption, directory2.GetPath().wstring() } });
		ASSERT_TRUE(static_cast<bool>(options));
		ASSERT_NE(nullptr, options->GetStartInfo());

		auto workingDirectory = options->GetStartInfo()->GetWorkingDirectory();
		ASSERT_NE(nullptr, workingDirectory);
		ASSERT_EQ(directory2.GetPath(), *workingDirectory);
	}

	//-------------------------------------------------------------------------
	TEST(OptionsParserConfigTest, ConfigurationFileMerge)
	{
		const std::wstring source1 = L"s1";
		const std::wstring source2 = L"s2";

		auto options = MutipleSourceParse(
		{ { cov::ProgramOptions::SelectedSourcesOption, source1 } },
		{ { cov::ProgramOptions::SelectedSourcesOption, source2 } });
		ASSERT_TRUE(static_cast<bool>(options));
		// The config file is the base; command-line values are appended.
		const auto& sourcePatterns = options->GetSourcePatterns().GetSelectedPatterns();
		ASSERT_EQ(2, sourcePatterns.size());
		ASSERT_EQ(source1, sourcePatterns[0]);
		ASSERT_EQ(source2, sourcePatterns[1]);
	}

	//-------------------------------------------------------------------------
	TEST(OptionsParserConfigTest, InvalidConfigFile)
	{
		cov::OptionsParser parser;
		TestHelper::TemporaryPath path;
		{
			std::ofstream ofs(path.GetPath().string(), std::ios::binary);
			ofs << "this is not valid json";
		}

		auto options = TestTools::Parse(
			parser,
			{ "--" + cov::ProgramOptions::ConfigFileOption,
			  path.GetPath().string() });

		ASSERT_FALSE(static_cast<bool>(options));
	}
}
