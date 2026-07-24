// OpenCppCoverage is an open source code coverage for C++.
// Copyright (C) 2014 OpenCppCoverage

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

#pragma once

#include "TaleaCppCovExport.hpp"
#include <unordered_map>
#include "CoverageRate.hpp"

namespace Coverage
{
	class ModuleCoverage;
	class FileCoverage;
	class CoverageData;
}

namespace TaleaCppCov
{
	class TALEACPPCOV_DLL CoverageRateComputer
	{
	public:
		explicit CoverageRateComputer(const Coverage::CoverageData&);

		std::vector<Coverage::ModuleCoverage*> SortModulesByCoverageRate() const;
		std::vector<Coverage::FileCoverage*> SortFilesByCoverageRate(const Coverage::ModuleCoverage&) const;
		
		const CoverageRate& GetCoverageRate() const;
		const CoverageRate& GetCoverageRate(const Coverage::ModuleCoverage&) const;
		const CoverageRate& GetCoverageRate(const Coverage::FileCoverage&) const;

	private:
		CoverageRateComputer(const CoverageRateComputer&) = delete;
		CoverageRateComputer& operator=(const CoverageRateComputer&) = delete;

		void ComputeCoverageRateCache(const Coverage::CoverageData& coverageData);

		const Coverage::CoverageData& coverageData_;

		std::unordered_map<const Coverage::ModuleCoverage*, CoverageRate> moduleCoverageRate_;
		std::unordered_map<const Coverage::FileCoverage*, CoverageRate> fileCoverageRate_;
		CoverageRate coverageRate_;
	};
}