// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2014 OpenCppCoverage

#pragma once
#include <cstdint>

#include "TaleaCppCovExport.hpp"

namespace TaleaCppCov
{
	class TALEACPPCOV_DLL CoverageRate
	{
	public:
		CoverageRate();
		CoverageRate(std::int32_t executedLinesCount, std::int32_t unexecutedLinesCount);
		CoverageRate(std::int32_t executedLinesCount,
		             std::int32_t unexecutedLinesCount,
		             std::int32_t coveredBranchCount,
		             std::int32_t totalBranchCount);

		CoverageRate& operator=(const CoverageRate&) = delete;
		CoverageRate(const CoverageRate&) = default;

		std::int32_t GetExecutedLinesCount() const;
		std::int32_t GetUnExecutedLinesCount() const;
		std::int32_t GetTotalLinesCount() const;
		std::int32_t GetPercentRate() const;
		double GetRate() const;

		std::int32_t GetCoveredBranchCount() const;
		std::int32_t GetTotalBranchCount() const;
		double GetBranchRate() const;

		CoverageRate& operator+=(const CoverageRate&);

	private:
		std::int32_t executedLinesCount_;
		std::int32_t unexecutedLinesCount_;
		std::int32_t coveredBranchCount_;
		std::int32_t totalBranchCount_;
	};
}

