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

#include "CppCoverageExport.hpp"

namespace CppCoverage
{
	class CPPCOVERAGE_DLL CoverageRate
	{
	public:
		CoverageRate();
		CoverageRate(int executedLinesCount, int unexecutedLinesCount);
		CoverageRate(int executedLinesCount,
		             int unexecutedLinesCount,
		             int coveredBranchCount,
		             int totalBranchCount);

		CoverageRate& operator=(const CoverageRate&) = delete;
		CoverageRate(const CoverageRate&) = default;

		int GetExecutedLinesCount() const;
		int GetUnExecutedLinesCount() const;
		int GetTotalLinesCount() const;
		int GetPercentRate() const;
		double GetRate() const;

		int GetCoveredBranchCount() const;
		int GetTotalBranchCount() const;
		double GetBranchRate() const;

		CoverageRate& operator+=(const CoverageRate&);

	private:
		int executedLinesCount_;
		int unexecutedLinesCount_;
		int coveredBranchCount_;
		int totalBranchCount_;
	};
}

