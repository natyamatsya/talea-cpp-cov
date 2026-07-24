// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2014 OpenCppCoverage

#include "stdafx.h"
#include <cstdint>

#include <memory>

#include "talea-cpp-cov/CoverageRateComputer.hpp"
#include "coverage/CoverageData.hpp"
#include "coverage/ModuleCoverage.hpp"
#include "coverage/FileCoverage.hpp"
#include "talea-cpp-cov/CoverageRate.hpp"

namespace cov = TaleaCppCov;

namespace TaleaCppCovTest
{
	namespace
	{
		//---------------------------------------------------------------------
		struct CoverageRateComputerTest : public testing::Test
		{
			//-----------------------------------------------------------------
			CoverageRateComputerTest()
				: coverageData_(L"", 0)				
			{				
				module1_ = &coverageData_.AddModule(L"m1");
				module2_ = &coverageData_.AddModule(L"m2");

				file1_ = &module1_->AddFile(L"f1");
				file2_ = &module1_->AddFile(L"f2");
				file3_ = &module2_->AddFile(L"f3");

				SetFileLineCount(*file1_, 3, 4);
				SetFileLineCount(*file2_, 1, 2);
				SetFileLineCount(*file3_, 5, 10);

				coverageRateComputer_.reset(new cov::CoverageRateComputer{ coverageData_ });
			}

			//-----------------------------------------------------------------
			static void SetFileLineCount(
				Coverage::FileCoverage& file, 
				std::int32_t executedCount, 
				std::int32_t unexecutedCount)
			{
				std::int32_t line = 0;

				for (std::int32_t i = 0; i < executedCount; ++i)
					file.AddLine(++line, true);
				for (std::int32_t i = 0; i < unexecutedCount; ++i)
					file.AddLine(++line, false);
			}

			Coverage::CoverageData coverageData_;
			std::unique_ptr<cov::CoverageRateComputer> coverageRateComputer_;
			Coverage::ModuleCoverage* module1_;
			Coverage::ModuleCoverage* module2_;
			Coverage::FileCoverage* file1_;
			Coverage::FileCoverage* file2_;
			Coverage::FileCoverage* file3_;
		};
	}

	//-------------------------------------------------------------------------
	TEST_F(CoverageRateComputerTest, FileCoverage)
	{
		const auto& rate = coverageRateComputer_->GetCoverageRate(*file2_);

		ASSERT_EQ(1, rate.GetExecutedLinesCount());
		ASSERT_EQ(2, rate.GetUnExecutedLinesCount());
	}

	//-------------------------------------------------------------------------
	TEST_F(CoverageRateComputerTest, ModuleCoverage)
	{
		const auto& rate = coverageRateComputer_->GetCoverageRate(*module1_);

		ASSERT_EQ(1 + 3, rate.GetExecutedLinesCount());
		ASSERT_EQ(2 + 4, rate.GetUnExecutedLinesCount());
	}

	//-------------------------------------------------------------------------
	TEST_F(CoverageRateComputerTest, CoverageRate)
	{
		const auto& rate = coverageRateComputer_->GetCoverageRate();

		ASSERT_EQ(1 + 3 + 5, rate.GetExecutedLinesCount());
		ASSERT_EQ(2 + 4 + 10, rate.GetUnExecutedLinesCount());
	}

	//-------------------------------------------------------------------------
	TEST_F(CoverageRateComputerTest, SortModulesByCoverageRate)
	{
		const auto& modules = coverageRateComputer_->SortModulesByCoverageRate();

		ASSERT_EQ(2, modules.size());
		ASSERT_EQ(module2_->GetPath(), modules[0]->GetPath());
		ASSERT_EQ(module1_->GetPath(), modules[1]->GetPath());		
	}

	//-------------------------------------------------------------------------
	TEST_F(CoverageRateComputerTest, SortFileByCoverageRate)
	{
		const auto& files = coverageRateComputer_->SortFilesByCoverageRate(*module1_);

		ASSERT_EQ(2, files.size());
		ASSERT_EQ(file2_->GetPath(), files[0]->GetPath());
		ASSERT_EQ(file1_->GetPath(), files[1]->GetPath());
	}
}