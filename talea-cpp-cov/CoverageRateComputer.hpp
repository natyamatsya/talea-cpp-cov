// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2014 OpenCppCoverage

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