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

#include "stdafx.h"
#include <cstdint>
#include "CoverageRateComputer.hpp"

#include "coverage/CoverageData.hpp"
#include "coverage/ModuleCoverage.hpp"
#include "coverage/FileCoverage.hpp"
#include "CoverageRate.hpp"

namespace TaleaCppCov
{
	namespace
	{
		//---------------------------------------------------------------------
		CoverageRate ComputeFileCoverage(const Coverage::FileCoverage& file)
		{
			std::int32_t executedLines = 0;
			std::int32_t unexecutedLines = 0;
			std::int32_t coveredBranches = 0;
			std::int32_t totalBranches = 0;

			for (const auto& [lineNumber, lineCoverage] : file.GetLineCollection())
			{
				if (lineCoverage.HasBeenExecuted())
					++executedLines;
				else
					++unexecutedLines;

				for (const auto& branch : lineCoverage.GetBranches())
				{
					// A conditional branch has two outcomes:
					// branch taken and fall through.
					totalBranches += 2;
					if (branch.HasBeenTaken())
						++coveredBranches;
					if (branch.HasBeenFallenThrough())
						++coveredBranches;
				}
			}

			return CoverageRate{
			    executedLines, unexecutedLines, coveredBranches, totalBranches};
		}
		
		//---------------------------------------------------------------------
		template<typename Object>
		std::vector<Object*> SortByCoverageRate(
			const std::vector<std::unique_ptr<Object>>& objects,
			const std::unordered_map<const Object*, CoverageRate>& coverageRates)
		{
			std::vector<Object*> sortedObjects;
			
			for (const auto& object : objects)
				sortedObjects.push_back(object.get());

			std::sort(sortedObjects.begin(), sortedObjects.end(), 
				[&](const Object* object1, const Object* object2)
			{
				return coverageRates.at(object1).GetPercentRate() 
					 < coverageRates.at(object2).GetPercentRate();
			});

			return sortedObjects;
		}
	}

	//-------------------------------------------------------------------------
	CoverageRateComputer::CoverageRateComputer(const Coverage::CoverageData& coverageData)
		: coverageData_(coverageData)
	{
		ComputeCoverageRateCache(coverageData);
	}
	
	//-------------------------------------------------------------------------
	void CoverageRateComputer::ComputeCoverageRateCache(const Coverage::CoverageData& coverageData)
	{
		for (const auto& module : coverageData.GetModules())
		{
			CoverageRate moduleCoverageRate;

			for (const auto& file : module->GetFiles())
			{
				CoverageRate fileCoverageRate = ComputeFileCoverage(*file);

				moduleCoverageRate += fileCoverageRate;
				fileCoverageRate_.emplace(file.get(), fileCoverageRate);
			}

			moduleCoverageRate_.emplace(module.get(), moduleCoverageRate);
			coverageRate_ += moduleCoverageRate;
		}
	}
	
	//-------------------------------------------------------------------------
	std::vector<Coverage::ModuleCoverage*> CoverageRateComputer::SortModulesByCoverageRate() const
	{
		return SortByCoverageRate(coverageData_.GetModules(), moduleCoverageRate_);
	}

	//-------------------------------------------------------------------------
	std::vector<Coverage::FileCoverage*> CoverageRateComputer::SortFilesByCoverageRate(
		const Coverage::ModuleCoverage& modules) const
	{
		return SortByCoverageRate(modules.GetFiles(), fileCoverageRate_);
	}

	//-------------------------------------------------------------------------
	const CoverageRate& CoverageRateComputer::GetCoverageRate() const
	{
		return coverageRate_;
	}

	//-------------------------------------------------------------------------
	const CoverageRate& CoverageRateComputer::GetCoverageRate(const Coverage::ModuleCoverage& module) const
	{
		return moduleCoverageRate_.at(&module);
	}
	
	//-------------------------------------------------------------------------
	const CoverageRate& CoverageRateComputer::GetCoverageRate(const Coverage::FileCoverage& file) const
	{
		return fileCoverageRate_.at(&file);
	}
}
