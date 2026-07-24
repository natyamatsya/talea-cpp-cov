// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2014 OpenCppCoverage

#include "stdafx.h"

#include <functional>

#include "talea-cpp-cov/WildcardCoverageFilter.hpp"
#include "talea-cpp-cov/CoverageFilterSettings.hpp"
#include "talea-cpp-cov/Patterns.hpp"

namespace cov = TaleaCppCov;

namespace TaleaCppCovTest
{
	namespace
	{
		//-------------------------------------------------------------------------
		struct WildcardCoverageFilterTest : public ::testing::Test
		{
			//---------------------------------------------------------------------
			WildcardCoverageFilterTest()
			: emptyPatterns_{}
			, defaultPatterns_{BuildDefaultPatterns()}
			{

			}

			//---------------------------------------------------------------------
			cov::Patterns BuildDefaultPatterns()
			{
				cov::Patterns patterns{false};

				patterns.AddSelectedPatterns(L"a*b");
				patterns.AddExcludedPatterns(L"3");

				return patterns;
			}

			//---------------------------------------------------------------------
			void CheckSelection(
				cov::CoverageFilterSettings& settings, 
				std::function<bool(const cov::WildcardCoverageFilter&, const std::wstring&)> isSelected)
			{
				cov::WildcardCoverageFilter filter{settings};

				ASSERT_FALSE(isSelected(filter, L"aa"));
				ASSERT_FALSE(isSelected(filter, L"a3b"));
				ASSERT_TRUE(isSelected(filter, L"ab"));			
			}	

			cov::Patterns emptyPatterns_;
			cov::Patterns defaultPatterns_;
		};
	}

	//-------------------------------------------------------------------------
	TEST_F(WildcardCoverageFilterTest, IsModuleSelected)
	{
		cov::CoverageFilterSettings settings{ defaultPatterns_, emptyPatterns_ };
		CheckSelection(settings, [](const cov::WildcardCoverageFilter& filter, const std::wstring& str)
		{
			return filter.IsModuleSelected(str);
		});
	}

	//-------------------------------------------------------------------------
	TEST_F(WildcardCoverageFilterTest, IsSourceSelected)
	{
		cov::CoverageFilterSettings settings{ emptyPatterns_, defaultPatterns_ };
		CheckSelection(settings, [](const cov::WildcardCoverageFilter& filter, const std::wstring& str)
		{
			return filter.IsSourceFileSelected(str);
		});		
	}
}