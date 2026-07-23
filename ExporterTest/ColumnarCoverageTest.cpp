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

#include <fstream>

#include "Coverage/CoverageData.hpp"
#include "Coverage/ModuleCoverage.hpp"
#include "Coverage/FileCoverage.hpp"
#include "Coverage/LineCoverage.hpp"

#include "Exporter/Binary/ColumnarCoverage.hpp"

#include "TestHelper/TemporaryPath.hpp"

using namespace testing;

namespace ExporterTest
{
	namespace
	{
		//---------------------------------------------------------------------
		const Coverage::FileCoverage& GetOnlyFile(
		    const Coverage::CoverageData& coverageData)
		{
			const auto& modules = coverageData.GetModules();
			EXPECT_EQ(1u, modules.size());
			const auto& files = modules.front()->GetFiles();
			EXPECT_EQ(1u, files.size());
			return *files.front();
		}

		//---------------------------------------------------------------------
		// One-module/one-file CoverageData whose lines come from the (line,
		// executed) pairs given.
		Coverage::CoverageData MakeCoverage(
		    const std::wstring& name,
		    std::int32_t exitCode,
		    const std::vector<std::pair<std::uint32_t, bool>>& lines)
		{
			Coverage::CoverageData coverageData{name, exitCode};
			auto& file = coverageData.AddModule(L"module.dll").AddFile(L"file.cpp");
			for (const auto& [line, executed] : lines)
				file.AddLine(line, executed);
			return coverageData;
		}
	}

	//-------------------------------------------------------------------------
	// Same build (identical line arrays): merging OR-s the executed bits.
	TEST(ColumnarCoverageTest, MergeSameBuildOrsExecutedBits)
	{
		Exporter::ColumnarCoverageMerger merger;
		merger.Add(MakeCoverage(L"a", 0, {{10, true}, {20, false}, {30, false}}));
		merger.Add(MakeCoverage(L"b", 0, {{10, false}, {20, true}, {30, false}}));

		auto merged = merger.ToCoverageData();
		const auto& file = GetOnlyFile(merged);

		ASSERT_NE(nullptr, file[10]);
		EXPECT_TRUE(file[10]->HasBeenExecuted());  // from a
		EXPECT_TRUE(file[20]->HasBeenExecuted());  // from b
		EXPECT_FALSE(file[30]->HasBeenExecuted()); // neither
	}

	//-------------------------------------------------------------------------
	// Last Add wins the name; the last non-zero exit code is kept.
	TEST(ColumnarCoverageTest, MergeKeepsNameAndLastNonZeroExitCode)
	{
		Exporter::ColumnarCoverageMerger merger;
		merger.Add(MakeCoverage(L"first", 7, {{1, true}}));
		merger.Add(MakeCoverage(L"second", 0, {{1, true}}));

		auto merged = merger.ToCoverageData();
		EXPECT_EQ(L"second", merged.GetName());
		EXPECT_EQ(7, merged.GetExitCode());
	}

	//-------------------------------------------------------------------------
	// Different builds (different line sets): merging unions the lines.
	TEST(ColumnarCoverageTest, MergeDifferentBuildUnionsLines)
	{
		Exporter::ColumnarCoverageMerger merger;
		merger.Add(MakeCoverage(L"a", 0, {{10, true}, {20, false}}));
		merger.Add(MakeCoverage(L"b", 0, {{20, true}, {40, true}}));

		auto merged = merger.ToCoverageData();
		const auto& file = GetOnlyFile(merged);

		ASSERT_NE(nullptr, file[10]);
		ASSERT_NE(nullptr, file[20]);
		ASSERT_NE(nullptr, file[40]);
		EXPECT_TRUE(file[10]->HasBeenExecuted());
		EXPECT_TRUE(file[20]->HasBeenExecuted()); // false | true
		EXPECT_TRUE(file[40]->HasBeenExecuted());
	}

	//-------------------------------------------------------------------------
	// Branch outcomes OR by (line, branchIndex) across shards.
	TEST(ColumnarCoverageTest, MergeOrsBranchOutcomes)
	{
		Coverage::CoverageData a{L"a", 0};
		auto& fileA = a.AddModule(L"m.dll").AddFile(L"f.cpp");
		fileA.AddLine(5, true);
		fileA.AddBranch(5, Coverage::BranchCoverage{0, true, false});

		Coverage::CoverageData b{L"b", 0};
		auto& fileB = b.AddModule(L"m.dll").AddFile(L"f.cpp");
		fileB.AddLine(5, true);
		fileB.AddBranch(5, Coverage::BranchCoverage{0, false, true});

		Exporter::ColumnarCoverageMerger merger;
		merger.Add(a);
		merger.Add(b);

		auto merged = merger.ToCoverageData();
		const auto& file = GetOnlyFile(merged);
		ASSERT_NE(nullptr, file[5]);
		const auto& branches = file[5]->GetBranches();
		ASSERT_EQ(1u, branches.size());
		EXPECT_TRUE(branches[0].HasBeenTaken());          // from a
		EXPECT_TRUE(branches[0].HasBeenFallenThrough());  // from b
	}

	//-------------------------------------------------------------------------
	// A columnar file written by Write is merged straight from disk by
	// AddColumnarFile, OR-ing against in-memory coverage.
	TEST(ColumnarCoverageTest, AddColumnarFileMergesFromDisk)
	{
		TestHelper::TemporaryPath path;
		{
			auto onDisk = MakeCoverage(L"disk", 0, {{1, true}, {2, false}});
			std::ofstream ofs{path.GetPath().string(), std::ios::binary};
			Tools::ThrowOnError(Exporter::ColumnarBinary::Write(onDisk, ofs));
		}

		Exporter::ColumnarCoverageMerger merger;
		merger.Add(MakeCoverage(L"mem", 0, {{1, false}, {2, true}}));
		ASSERT_TRUE(merger.AddColumnarFile(path.GetPath()));

		auto merged = merger.ToCoverageData();
		const auto& file = GetOnlyFile(merged);
		EXPECT_TRUE(file[1]->HasBeenExecuted());
		EXPECT_TRUE(file[2]->HasBeenExecuted());
	}

	//-------------------------------------------------------------------------
	// AddColumnarFile rejects non-columnar files so the caller can fall back.
	TEST(ColumnarCoverageTest, AddColumnarFileRejectsNonColumnar)
	{
		TestHelper::TemporaryPath path;
		{
			std::ofstream ofs{path.GetPath().string(), std::ios::binary};
			ofs << "not a columnar file";
		}

		Exporter::ColumnarCoverageMerger merger;
		EXPECT_FALSE(merger.AddColumnarFile(path.GetPath()));
	}
}
