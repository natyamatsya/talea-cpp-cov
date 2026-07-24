// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2018 OpenCppCoverage

#include "stdafx.h"
#include <cstdint>

#include "talea-cpp-cov/FilterAssistant.hpp"
#include <optional>
#include <filesystem>
#include "FileSystemMock.hpp"

namespace cov = TaleaCppCov;
using namespace testing;

namespace
{
	//-------------------------------------------------------------------------
	struct Params
	{
		void (cov::FilterAssistant::*onNewFile)(const std::filesystem::path&,
		                                        bool);
		std::optional<std::filesystem::path> (
		    cov::FilterAssistant::*computeSuggestedFilter)() const;
	};
}

namespace TaleaCppCovTest
{
	//-------------------------------------------------------------------------
	struct FilterAssistantTest : public TestWithParam<Params>
	{
		//---------------------------------------------------------------------
		FilterAssistantTest() : fileSystem_{std::make_shared<FileSystemMock>()}
		{
			filterAssistant_ = std::make_unique<cov::FilterAssistant>(
			    std::shared_ptr<cov::IFileSystem>{fileSystem_});
		}

		//---------------------------------------------------------------------
		void AddFile(const std::filesystem::path& path,
					 std::int32_t time,
		             bool isSelected)
		{
			EXPECT_CALL(*fileSystem_, GetLastWriteTime(path))
			    .WillRepeatedly(Return(std::filesystem::file_time_type(std::chrono::seconds(time))));
			(*filterAssistant_.*GetParam().onNewFile)(path, isSelected);
		}

		//---------------------------------------------------------------------
		std::optional<std::filesystem::path> ComputeSuggestedFilter() const
		{
			return (*filterAssistant_.*GetParam().computeSuggestedFilter)();
		}

	  private:
		std::shared_ptr<FileSystemMock> fileSystem_;
		std::unique_ptr<cov::FilterAssistant> filterAssistant_;
	};

	//-------------------------------------------------------------------------
	TEST_P(FilterAssistantTest, ComputeSuggestSourceFile)
	{
		ASSERT_EQ(std::nullopt, ComputeSuggestedFilter());

		AddFile("folder1/file1", 1, false);
		AddFile("folder3/file3", 3, false);
		AddFile("folder2/file2", 2, false);

		auto suggestedFile = ComputeSuggestedFilter();
		ASSERT_TRUE(suggestedFile);
		ASSERT_EQ(L"folder3", suggestedFile->wstring());

		AddFile("file4", 2, true);
		AddFile("file5", 2, false);
		ASSERT_EQ(std::nullopt, ComputeSuggestedFilter());
	}

	//-------------------------------------------------------------------------
	INSTANTIATE_TEST_SUITE_P(
	    FilterAssistantTest,
	    FilterAssistantTest,
	    ::testing::Values(
	        Params{&cov::FilterAssistant::OnNewModule,
	               &cov::FilterAssistant::ComputeSuggestedModuleFilter},
	        Params{&cov::FilterAssistant::OnNewSourceFile,
	               &cov::FilterAssistant::ComputeSuggestedSourceFileFilter}));
}
