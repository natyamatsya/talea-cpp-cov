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

#pragma once
#include <cstdint>

#include "CppCoverageExport.hpp"

namespace CppCoverage
{
	class CPPCOVERAGE_DLL CoverageRate
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

