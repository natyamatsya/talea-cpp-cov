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

#include <random>

#include "CppCoverage/CoverageDataMerger.hpp"
#include "Coverage/CoverageData.hpp" 
#include "Coverage/ModuleCoverage.hpp" 
#include "Coverage/FileCoverage.hpp" 
#include "Coverage/LineCoverage.hpp" 

namespace cov = CppCoverage;
namespace fs = std::filesystem;

namespace CppCoverageTest
{
	namespace
	{
		//---------------------------------------------------------------------
		struct LineInfo
		{
			LineInfo(
				const fs::path& module,
				const fs::path& file,
				std::uint32_t line)
				: module_(module)
				, file_(file)
				, line_{ line }
			{
			}

			//---------------------------------------------------------------------
			bool operator<(const LineInfo& lineInfo) const
			{
				return std::make_tuple(module_, file_, line_)
					< std::make_tuple(lineInfo.module_, lineInfo.file_, lineInfo.line_);
			}

			fs::path module_;
			fs::path file_;
			std::uint32_t line_;
		};
		
		using LineInfoHasBeenExecuted = std::map<LineInfo, std::uint32_t>;
		using RandFct = std::function < std::uint32_t() > ;

		//---------------------------------------------------------------------
		std::set<std::uint32_t> GetRandomValues(const RandFct& getRand, size_t elementNumber)
		{
			std::set<std::uint32_t> randomValues;

			while (randomValues.size() < elementNumber)
				randomValues.insert(getRand());
			return randomValues;
		}

		//---------------------------------------------------------------------
		void AddRandomFileCoverage(
			const RandFct& getRand,
			std::uint32_t fileIndex,
			Coverage::ModuleCoverage& module,			
			LineInfoHasBeenExecuted& mergedLineInfo)
		{
			auto fileName = std::to_wstring(fileIndex);
			auto& file = module.AddFile(fileName);
			
			for (auto line: GetRandomValues(getRand, getRand()))
			{
				auto hasBeenExecuted = getRand() % 2;

				file.AddLine(line, (hasBeenExecuted > 0));
				LineInfo lineInfo{ module.GetPath().wstring(), fileName, line };
			
				mergedLineInfo[lineInfo] += hasBeenExecuted; // Count the execution
			}
		}

		//---------------------------------------------------------------------
		Coverage::CoverageData AddRandomCoverageData(
			const RandFct& getRand,
			LineInfoHasBeenExecuted& mergedLineInfo)
		{
			Coverage::CoverageData coverageData{ L"", 0 };
			
			for (auto moduleIndex: GetRandomValues(getRand, getRand()))
			{
				auto moduleName = std::to_wstring(moduleIndex);
				auto& module = coverageData.AddModule(moduleName);
				
				for (auto fileIndex: GetRandomValues(getRand, getRand()))
					AddRandomFileCoverage(getRand, fileIndex, module, mergedLineInfo);
			}

			return coverageData;
		}

		//---------------------------------------------------------------------
		std::vector<Coverage::CoverageData> AddRandomCoverageDataCollection(
			std::int32_t maxRandomValue,
			LineInfoHasBeenExecuted& mergedLineInfo)
		{
			std::default_random_engine generator;
			std::uniform_int_distribution<std::uint32_t> distribution(1, maxRandomValue);
			auto getRand = [&](){ return distribution(generator); };

			std::vector<Coverage::CoverageData> coverageDatas;
			auto coverageDataCount = getRand();

			for (size_t i = 0; i < coverageDataCount; ++i)
				coverageDatas.emplace_back(AddRandomCoverageData(getRand, mergedLineInfo));
			
			return coverageDatas;
		}
	}

	//-------------------------------------------------------------------------
	TEST(CoverageDataMergerRandomTest, RandomTest)
	{
		LineInfoHasBeenExecuted mergedLineInfo;
		
		auto coverageDatas = AddRandomCoverageDataCollection(6, mergedLineInfo);
		auto coverageDataMerged = cov::CoverageDataMerger{}.Merge(coverageDatas);
		size_t totalLine = 0;

		for (const auto& module : coverageDataMerged.GetModules())
		{
			for (const auto& file : module->GetFiles())
			{
				for (const auto& lineCoverage : file->GetLines())
				{
					const auto& executedCount = mergedLineInfo.at(
						{ module->GetPath(), file->GetPath(), lineCoverage.GetLineNumber() });
					auto hasBeenExecuted = executedCount > 0;
					ASSERT_EQ(hasBeenExecuted, lineCoverage.HasBeenExecuted());
					++totalLine;
				}
			}
		}

		ASSERT_EQ(mergedLineInfo.size(), totalLine);
	}
}