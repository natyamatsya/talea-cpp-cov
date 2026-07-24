// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2014 OpenCppCoverage

#include "stdafx.h"
#include <cstdint>

#include "talea-cpp-cov/CoverageRate.hpp"

namespace cov = TaleaCppCov;

namespace TaleaCppCovTest
{
	//-------------------------------------------------------------------------
	TEST(CoverageRateTest, BasicAccessors)
	{
		const std::int32_t executedLinesCount = 42;
		const std::int32_t unexecutedLinesCount = 10;

		cov::CoverageRate rate{ executedLinesCount, unexecutedLinesCount };

		ASSERT_EQ(executedLinesCount + unexecutedLinesCount, rate.GetTotalLinesCount());
		ASSERT_EQ((100 * executedLinesCount) / (executedLinesCount + unexecutedLinesCount), rate.GetPercentRate());
	}

	//-------------------------------------------------------------------------
	TEST(CoverageRateTest, Add)
	{		
		cov::CoverageRate rate1{ 42, 10 };
		cov::CoverageRate rate2{ 12, 53 };

		cov::CoverageRate sum{ rate1 };

		sum += rate2;

		ASSERT_EQ(rate1.GetExecutedLinesCount() + rate2.GetExecutedLinesCount(), sum.GetExecutedLinesCount());
		ASSERT_EQ(rate1.GetUnExecutedLinesCount() + rate2.GetUnExecutedLinesCount(), sum.GetUnExecutedLinesCount());
	}
}