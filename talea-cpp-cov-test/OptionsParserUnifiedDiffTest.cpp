// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2016 OpenCppCoverage

#include "stdafx.h"

#include "talea-cpp-cov/OptionsParser.hpp"
#include "talea-cpp-cov/ProgramOptions.hpp"
#include "talea-cpp-cov/UnifiedDiffSettings.hpp"

#include "talea-cpp-cov-test/TestTools.hpp"
#include "test-helper/TemporaryPath.hpp"

namespace cov = TaleaCppCov;
namespace fs = std::filesystem;

namespace TaleaCppCovTest
{
	namespace
	{
		//-------------------------------------------------------------------------
		std::optional<cov::Options> Parse(
			const std::vector<cov::UnifiedDiffSettings>& unifiedDiffSettingsCollection)
		{
			cov::OptionsParser parser;
			std::wostringstream ostr;
			std::vector<std::string> arguments;

			for (const auto& unifiedDiffSettings : unifiedDiffSettingsCollection)
			{
				arguments.push_back(TestTools::GetOptionPrefix() + cov::ProgramOptions::UnifiedDiffOption);

				auto value = unifiedDiffSettings.GetUnifiedDiffPath().string();
				auto rootDiffFolder = unifiedDiffSettings.GetRootDiffFolder();
				if (rootDiffFolder)
					value += cov::OptionsParser::PathSeparator + rootDiffFolder->string();
				arguments.push_back(value);
			}

			auto option = TestTools::Parse(parser, arguments, true, &ostr);

			if (!option && ostr.str().empty())
				throw std::runtime_error("Expect error message.");
			return option;
		}


		//-------------------------------------------------------------------------
		std::vector<cov::UnifiedDiffSettings> ToVector(cov::UnifiedDiffSettings&& unifiedDiffSettings)
		{
			std::vector<cov::UnifiedDiffSettings> unifiedDiffSettingsCollection;

			unifiedDiffSettingsCollection.push_back(std::move(unifiedDiffSettings));
			return unifiedDiffSettingsCollection;
		}

		//-------------------------------------------------------------------------
		std::optional<cov::Options> Parse(cov::UnifiedDiffSettings&& unifiedDiffSettings)
		{
			return Parse(ToVector(std::move(unifiedDiffSettings)));
		}

		//---------------------------------------------------------------------
		bool AreEqual(
			const std::vector<cov::UnifiedDiffSettings>& settings1,
			const std::vector<cov::UnifiedDiffSettings>& settings2)
		{
			return std::equal(settings1.begin(), settings1.end(), settings2.begin(), settings2.end(),
				[](const auto& setting1, const auto& setting2)
			{
				return setting1.GetUnifiedDiffPath() == setting2.GetUnifiedDiffPath()
					&& setting1.GetRootDiffFolder() == setting2.GetRootDiffFolder();
			});
		}

		//---------------------------------------------------------------------
		void CheckUnifiedDiffSettings(const std::vector<cov::UnifiedDiffSettings>& unifiedDiffSettingsCollection)
		{
			auto option = Parse(unifiedDiffSettingsCollection);

			ASSERT_TRUE(!!option);
			ASSERT_TRUE(AreEqual(unifiedDiffSettingsCollection, option->GetUnifiedDiffSettingsCollection()));
		}

		//---------------------------------------------------------------------
		void CheckUnifiedDiffSettings(cov::UnifiedDiffSettings&& unifiedDiffSettings)
		{
			CheckUnifiedDiffSettings(ToVector(std::move(unifiedDiffSettings)));
		}

		//---------------------------------------------------------------------
		struct OptionsParserUnifiedDifftTest : public testing::Test
		{
			TestHelper::TemporaryPath temporaryPath{ TestHelper::TemporaryPathOption::CreateAsFile };
		};
	}

	//-------------------------------------------------------------------------
	TEST_F(OptionsParserUnifiedDifftTest, UnifiedDiffPath)
	{
		CheckUnifiedDiffSettings({ temporaryPath, std::nullopt });
	}

	//-------------------------------------------------------------------------
	TEST_F(OptionsParserUnifiedDifftTest, NotFoundUnifiedDiffPath)
	{
		ASSERT_TRUE(!Parse({ "Unknow", std::nullopt }));
	}

	//-------------------------------------------------------------------------
	TEST_F(OptionsParserUnifiedDifftTest, RootDiffFolder)
	{
		CheckUnifiedDiffSettings({ temporaryPath, fs::current_path() });
	}

	//-------------------------------------------------------------------------
	TEST_F(OptionsParserUnifiedDifftTest, NotFoundRootDiffFolder)
	{
		ASSERT_TRUE(!!Parse({ temporaryPath, std::nullopt }));
		ASSERT_FALSE(!!Parse({ temporaryPath, fs::path("Unknow") }));
	}

	//-------------------------------------------------------------------------
	TEST_F(OptionsParserUnifiedDifftTest, TwoDiff)
	{
		std::vector<cov::UnifiedDiffSettings> unifiedDiffSettingsCollection;
		unifiedDiffSettingsCollection.emplace_back(temporaryPath, fs::current_path());
		unifiedDiffSettingsCollection.emplace_back(temporaryPath, std::nullopt);

		CheckUnifiedDiffSettings(unifiedDiffSettingsCollection);
	}
}