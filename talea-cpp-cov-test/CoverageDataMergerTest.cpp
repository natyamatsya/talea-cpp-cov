// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2014 OpenCppCoverage

#include "stdafx.h"
#include <cstdint>

#include <random>
#include <filesystem>

#include "talea-cpp-cov/CoverageDataMerger.hpp"
#include "coverage/CoverageData.hpp" 
#include "coverage/ModuleCoverage.hpp" 
#include "coverage/FileCoverage.hpp" 
#include "coverage/LineCoverage.hpp" 

namespace cov = TaleaCppCov;
namespace fs = std::filesystem;

namespace TaleaCppCovTest
{	
	namespace
	{
		//---------------------------------------------------------------------
		const fs::path modulePath = L"modulePath";
		const fs::path filePath = L"filePath";

		//---------------------------------------------------------------------
		std::vector<Coverage::CoverageData> CreateCoverageDataCollection(
			const std::initializer_list<std::pair<std::wstring, std::int32_t>>& coverageDataArguments)
		{
			std::vector<Coverage::CoverageData> coverageDataCollection;

			for (const auto& args : coverageDataArguments)
				coverageDataCollection.emplace_back(Coverage::CoverageData{ args.first, args.second });
			
			return coverageDataCollection;
		}

		//---------------------------------------------------------------------
		std::vector<Coverage::CoverageData> CreateCoverageDataCollection(size_t count)
		{
			std::vector<Coverage::CoverageData> coverageDataCollection;

			for (size_t i = 0; i < count; ++i)
				coverageDataCollection.emplace_back(Coverage::CoverageData{ L"", 0 });

			return coverageDataCollection;
		}

		//-------------------------------------------------------------------------
		void AddLinesToFileCoverage(
			Coverage::FileCoverage& fileCoverage,
			const std::initializer_list<std::pair<std::int32_t, bool>>& lineArgCollection)
		{
			for (const auto& lineArg : lineArgCollection)
				fileCoverage.AddLine(lineArg.first, lineArg.second);
		}

		//-------------------------------------------------------------------------
		void AddLine(
			Coverage::CoverageData& coverageData,
			const fs::path& modulePath,
			const fs::path& filePath,
			const std::initializer_list<std::pair<std::int32_t, bool>>& lineArgCollection)
		{
			auto& file = coverageData.AddModule(modulePath).AddFile(filePath);

			AddLinesToFileCoverage(file, lineArgCollection);
		}

		//-------------------------------------------------------------------------
		void CheckLineHasBeenExecuted(
			const std::unique_ptr<Coverage::FileCoverage>& file,
			std::int32_t lineNumber,
			bool exectedValue)
		{
			ASSERT_NE(nullptr, file);
			auto* line = (*file)[lineNumber];

			ASSERT_NE(nullptr, line);
			ASSERT_EQ(exectedValue, line->HasBeenExecuted());
		}
	}

	//-------------------------------------------------------------------------
	TEST(CoverageDataMergerTest, MergeBranches)
	{
		auto coverageDatas = CreateCoverageDataCollection(2);
		const std::int32_t lineNumber = 42;

		auto& file1 =
		    coverageDatas[0].AddModule(modulePath).AddFile(filePath);
		file1.AddLine(lineNumber, true);
		file1.AddBranch(lineNumber, Coverage::BranchCoverage{0, true, false});
		file1.AddBranch(lineNumber, Coverage::BranchCoverage{1, false, false});

		auto& file2 =
		    coverageDatas[1].AddModule(modulePath).AddFile(filePath);
		file2.AddLine(lineNumber, true);
		file2.AddBranch(lineNumber, Coverage::BranchCoverage{0, false, true});
		file2.AddBranch(lineNumber, Coverage::BranchCoverage{2, true, true});

		auto coverageDataMerged =
		    cov::CoverageDataMerger{}.Merge(coverageDatas);

		const auto& file =
		    coverageDataMerged.GetModules().at(0)->GetFiles().at(0);
		const auto* line = (*file)[lineNumber];
		ASSERT_NE(nullptr, line);

		const auto& branches = line->GetBranches();
		ASSERT_EQ(3u, branches.size());

		// Branch 0: outcomes are merged.
		ASSERT_TRUE(branches.at(0).HasBeenTaken());
		ASSERT_TRUE(branches.at(0).HasBeenFallenThrough());

		// Branch 1: only in the first coverage data.
		ASSERT_FALSE(branches.at(1).HasBeenTaken());
		ASSERT_FALSE(branches.at(1).HasBeenFallenThrough());

		// Branch 2: only in the second coverage data.
		ASSERT_TRUE(branches.at(2).HasBeenTaken());
		ASSERT_TRUE(branches.at(2).HasBeenFallenThrough());
	}

	//-------------------------------------------------------------------------
	TEST(CoverageDataMergerTest, CoverageData)
	{
		auto coverageDatas = CreateCoverageDataCollection({ { L"1", 0 }, { L"1", 0 } });
		auto coverageDataMerged = cov::CoverageDataMerger{}.Merge(coverageDatas);

		ASSERT_NE(std::wstring{}, coverageDataMerged.GetName());
		ASSERT_EQ(0, coverageDataMerged.GetExitCode());
	}

	//-------------------------------------------------------------------------
	TEST(CoverageDataMergerTest, NotZeroExitCode)
	{
		auto exitCode = 42;
		auto coverageDatas = CreateCoverageDataCollection({ { L"", 0 }, { L"", exitCode } });
		auto coverageDataMerged = cov::CoverageDataMerger{}.Merge(coverageDatas);

		ASSERT_EQ(exitCode, coverageDataMerged.GetExitCode());
	}
	
	//-------------------------------------------------------------------------
	TEST(CoverageDataMergerTest, Module)
	{
		auto coverageDatas = CreateCoverageDataCollection(2);

		AddLine(coverageDatas[0], "m1", "f1", {});
		AddLine(coverageDatas[1], "m2", "f2", {});

		auto coverageDataMerged = cov::CoverageDataMerger{}.Merge(coverageDatas);
		ASSERT_EQ(2, coverageDataMerged.GetModules().size());
	}

	//-------------------------------------------------------------------------
	TEST(CoverageDataMergerTest, File)
	{
		auto coverageDatas = CreateCoverageDataCollection(2);

		AddLine(coverageDatas[0], modulePath, "f1", {});
		AddLine(coverageDatas[1], modulePath, "f2", {});

		auto coverageDataMerged = cov::CoverageDataMerger{}.Merge(coverageDatas);
		const auto& mergedModule = coverageDataMerged.GetModules().at(0);
		ASSERT_NE(nullptr, mergedModule);
		ASSERT_EQ(2, mergedModule->GetFiles().size());
	}

	//-------------------------------------------------------------------------
	TEST(CoverageDataMergerTest, Line)
	{		
		auto coverageDatas = CreateCoverageDataCollection(2);				
				
		AddLine(coverageDatas[0], modulePath, filePath, { { 0, false }, { 1, false }, { 2, true } });
		AddLine(coverageDatas[1], modulePath, filePath, { { 1, true }, { 2, false }, { 3, true } });
		
		auto coverageDataMerged = cov::CoverageDataMerger{}.Merge(coverageDatas);
		const auto& mergedModule = coverageDataMerged.GetModules().at(0);
		ASSERT_NE(nullptr, mergedModule);
		const auto& mergedFile = mergedModule->GetFiles().at(0);
		ASSERT_NE(nullptr, mergedModule);
		ASSERT_EQ(4, mergedFile->GetLines().size());

		CheckLineHasBeenExecuted(mergedFile, 0, false);
		CheckLineHasBeenExecuted(mergedFile, 1, true);
		CheckLineHasBeenExecuted(mergedFile, 2, true);
		CheckLineHasBeenExecuted(mergedFile, 3, true);
	}

	//-------------------------------------------------------------------------
	TEST(CoverageDataMergerTest, AggregateByFileSingleModule)
	{
		Coverage::CoverageData coverageData{L"test", 0};

		AddLine(coverageData, modulePath, filePath, { { 0, false }, { 1, false }, { 2, true } });
		auto aggregated =
			cov::CoverageDataMerger{}.AggregateByFile(coverageData);

		// One synthetic module holding the file's coverage unchanged.
		ASSERT_EQ(1, aggregated.GetModules().size());
		auto& mergedFile = aggregated.GetModules().at(0)->GetFiles().at(0);
		ASSERT_EQ(3, mergedFile->GetLines().size());
		CheckLineHasBeenExecuted(mergedFile, 0, false);
		CheckLineHasBeenExecuted(mergedFile, 1, false);
		CheckLineHasBeenExecuted(mergedFile, 2, true);
	}

	//-------------------------------------------------------------------------
	TEST(CoverageDataMergerTest, AggregateByFileCollapsesModules)
	{
		Coverage::CoverageData coverageData{ L"test", 0 };

		auto& fileCoverage1 = coverageData.AddModule(modulePath).AddFile(filePath);
		auto& fileCoverage2 = coverageData.AddModule(L"otherModule").AddFile(filePath);

		AddLinesToFileCoverage(fileCoverage1, { { 0, false }, { 1, false }, { 2, true } });
		AddLinesToFileCoverage(fileCoverage2, { { 1, true }, { 2, false }, { 3, true } });
		auto aggregated =
			cov::CoverageDataMerger{}.AggregateByFile(coverageData);

		// The same source file in two modules collapses to ONE module with a
		// single file carrying the union of both modules' coverage.
		const auto& modules = aggregated.GetModules();
		ASSERT_EQ(1, modules.size());
		ASSERT_EQ(1, modules.at(0)->GetFiles().size());
		auto& mergedFile = modules.at(0)->GetFiles().at(0);
		ASSERT_EQ(4, mergedFile->GetLines().size());
		CheckLineHasBeenExecuted(mergedFile, 0, false);
		CheckLineHasBeenExecuted(mergedFile, 1, true);
		CheckLineHasBeenExecuted(mergedFile, 2, true);
		CheckLineHasBeenExecuted(mergedFile, 3, true);
	}

	//-------------------------------------------------------------------------
	TEST(CoverageDataMergerTest, AggregateByFileKeepsDistinctPaths)
	{
		Coverage::CoverageData coverageData{ L"test", 0 };

		AddLine(coverageData, modulePath, filePath, { { 0, true } });
		AddLine(coverageData, L"otherModule", L"other.cpp", { { 0, true } });

		auto aggregated =
			cov::CoverageDataMerger{}.AggregateByFile(coverageData);

		// Distinct source paths stay distinct files in the single module.
		ASSERT_EQ(1, aggregated.GetModules().size());
		ASSERT_EQ(2, aggregated.GetModules().at(0)->GetFiles().size());
	}
}