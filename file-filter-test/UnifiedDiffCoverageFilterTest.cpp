// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2016 OpenCppCoverage

#include "stdafx.h"

#include <optional>

#include "file-filter/UnifiedDiffCoverageFilter.hpp"
#include "file-filter/File.hpp"
#include "Tools.hpp"

using namespace FileFilter;

namespace FileFilterTest
{
	namespace
	{			
		const std::vector<std::wstring> fileNames_{ L"file1", L"file2" };			
	}

	//-------------------------------------------------------------------------
	TEST(UnifiedDiffCoverageFilterTest, IsSourceFileSelected)
	{			
		UnifiedDiffCoverageFilter filter{ ToFiles(fileNames_), std::nullopt };

		ASSERT_TRUE(filter.IsSourceFileSelected(fileNames_.at(0)));
		ASSERT_TRUE(filter.IsSourceFileSelected(fileNames_.at(0)));
		ASSERT_FALSE(filter.IsSourceFileSelected(L"Unknow"));
		ASSERT_TRUE(filter.IsSourceFileSelected(fileNames_.at(1)));
	}

	//-------------------------------------------------------------------------
	TEST(UnifiedDiffCoverageFilterTest, IsLineSelected)
	{
		auto files = ToFiles(fileNames_);
		const auto line = 42;

		files.at(0).AddSelectedLines({ line });
		UnifiedDiffCoverageFilter filter{std::move(files), std::nullopt };

		ASSERT_TRUE(filter.IsLineSelected(fileNames_.at(0), line));
		ASSERT_FALSE(filter.IsLineSelected(fileNames_.at(0), line + 1));
		ASSERT_FALSE(filter.IsLineSelected(fileNames_.at(1), line));
		ASSERT_FALSE(filter.IsLineSelected("Unknow", line));
	}

	//-------------------------------------------------------------------------
	TEST(UnifiedDiffCoverageFilterTest, AmbiguousPathException)
	{
		std::vector<std::wstring> filenames = { L"file1" };
		auto files = ToFiles(filenames);
				
		UnifiedDiffCoverageFilter filter{ std::move(files), std::nullopt };

		filter.IsSourceFileSelected(filenames.at(0));			
		ASSERT_THROW(filter.IsSourceFileSelected(L"Test/" + filenames.at(0)), std::runtime_error);
	}
}