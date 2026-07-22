// OpenCppCoverage is an open source code coverage for C++.
// Copyright (C) 2017 OpenCppCoverage
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

#include <fstream>
#include <map>
#include <tuple>

#include "CppCoverage/DebugInformationEnumerator.hpp"
#include "CppCoverage/PdbLineTableCache.hpp"
#include "CppCoverage/PdbSignature.hpp"
#include "TestCoverageConsole/TestDebugInformationEnumerator.hpp"
#include "TestCoverageConsole/TestCoverageConsole.hpp"
#include "TestHelper/TemporaryPath.hpp"

namespace CppCoverageTest
{
	namespace
	{
		struct DebugInformationHandlerMock
		    : CppCoverage::IDebugInformationHandler
		{
			//--------------------------------------------------------------------------
			explicit DebugInformationHandlerMock(
			    const std::filesystem::path& selectedFilename)
			    : selectedFilename_{selectedFilename}
			{
			}

			//--------------------------------------------------------------------------
			bool IsSourceFileSelected(
			    const std::filesystem::path& sourceFile) override
			{
				return selectedFilename_ == sourceFile.filename();
			}

			//--------------------------------------------------------------------------
			void OnSourceFile(const std::filesystem::path& path,
			                  const std::vector<Line>& lines) override
			{
				selectedFullPath_ = path;
				for (const auto& line : lines)
				{
					lines_.push_back(line.lineNumber_);
					if (line.length_ > 0)
						++lineWithLengthCount_;
				}
			}

			const std::filesystem::path selectedFilename_;
			std::filesystem::path selectedFullPath_;
			std::vector<int> lines_;
			int lineWithLengthCount_ = 0;
		};

		//---------------------------------------------------------------------------
		std::vector<int>
		GetLineNumbersWithTag(const std::filesystem::path& path,
		                      const std::wstring& tag)
		{
			std::vector<int> lines;
			std::wifstream ifs(path.wstring());
			std::wstring line;

			for (int lineNumber = 1; std::getline(ifs, line); ++lineNumber)
			{
				if (line.find(tag) != std::wstring::npos)
					lines.push_back(lineNumber);
			}

			return lines;
		}
	}

	//-------------------------------------------------------------------------
	TEST(DebugInformationEnumeratorTest, Enumerate)
	{
		auto selectedPath =
		    TestCoverageConsole::GetDebugInformationEnumeratorTestPath();
		DebugInformationHandlerMock debugInformationHandler{
		    selectedPath.filename()};

		CppCoverage::DebugInformationEnumerator debugInformationEnumerator{ {} };

		auto binary = TestCoverageConsole::GetOutputBinaryPath();
		ASSERT_TRUE(debugInformationEnumerator.Enumerate(
		    binary, debugInformationHandler));

		auto lineWithDebugInfo = GetLineNumbersWithTag(
		    debugInformationHandler.selectedFullPath_, L"@DebugInfoExpected");

		// The enumerator does not guarantee an order for the line records of
		// a file (the single-pass enumeration groups by source file id), so
		// compare the collected line numbers as an ordered set.
		std::sort(debugInformationHandler.lines_.begin(),
		          debugInformationHandler.lines_.end());
		ASSERT_EQ(debugInformationHandler.lines_, lineWithDebugInfo);
		ASSERT_NE(0, debugInformationHandler.lineWithLengthCount_);
	}

	namespace
	{
		//---------------------------------------------------------------------
		// Selects every source file and records the full delivered data, so
		// two enumerations can be compared for exact equality.
		struct RecordingHandler : CppCoverage::IDebugInformationHandler
		{
			using LineData = std::
			    tuple<unsigned long, int64_t, unsigned long, unsigned long>;

			bool IsSourceFileSelected(const std::filesystem::path&) override
			{
				return true;
			}

			void OnSourceFile(const std::filesystem::path& path,
			                  const std::vector<Line>& lines) override
			{
				auto& fileLines = files_[path.wstring()];
				for (const auto& line : lines)
				{
					fileLines.emplace_back(line.lineNumber_,
					                       line.virtualAddress_,
					                       line.symbolIndex_,
					                       line.length_);
				}
			}

			std::map<std::wstring, std::vector<LineData>> files_;
		};

		//---------------------------------------------------------------------
		RecordingHandler EnumerateBinary(
		    std::shared_ptr<CppCoverage::PdbLineTableCache> cache,
		    bool needSymbolIndex = false)
		{
			CppCoverage::DebugInformationEnumerator enumerator{
			    {}, needSymbolIndex, cache};
			RecordingHandler handler;
			EXPECT_TRUE(enumerator.Enumerate(
			    TestCoverageConsole::GetOutputBinaryPath(), handler));
			EXPECT_FALSE(handler.files_.empty());
			return handler;
		}
	}

	//-------------------------------------------------------------------------
	TEST(DebugInformationEnumeratorTest, PdbSignature)
	{
		auto signature = CppCoverage::ExtractPdbSignature(
		    TestCoverageConsole::GetOutputBinaryPath());
		ASSERT_TRUE(signature.has_value());
		ASSERT_FALSE(signature->ToString().empty());

		ASSERT_FALSE(CppCoverage::ExtractPdbSignature(L"does-not-exist.exe")
		                 .has_value());
	}

	//-------------------------------------------------------------------------
	TEST(DebugInformationEnumeratorTest, PdbLineTableCacheMemory)
	{
		auto cache = std::make_shared<CppCoverage::PdbLineTableCache>();

		auto firstRun = EnumerateBinary(cache);
		ASSERT_EQ(1, cache->GetMissCount());
		ASSERT_EQ(0, cache->GetMemoryHitCount());

		auto secondRun = EnumerateBinary(cache);
		ASSERT_EQ(1, cache->GetMissCount());
		ASSERT_EQ(1, cache->GetMemoryHitCount());

		ASSERT_EQ(firstRun.files_, secondRun.files_);
	}

	//-------------------------------------------------------------------------
	TEST(DebugInformationEnumeratorTest, PdbLineTableCacheDisk)
	{
		TestHelper::TemporaryPath cacheFolder{
		    TestHelper::TemporaryPathOption::CreateAsFolder};

		auto firstCache = std::make_shared<CppCoverage::PdbLineTableCache>(
		    cacheFolder.GetPath());
		auto firstRun = EnumerateBinary(firstCache);
		ASSERT_EQ(1, firstCache->GetMissCount());

		// A fresh cache on the same folder must hit the disk layer.
		auto secondCache = std::make_shared<CppCoverage::PdbLineTableCache>(
		    cacheFolder.GetPath());
		auto secondRun = EnumerateBinary(secondCache);
		ASSERT_EQ(0, secondCache->GetMissCount());
		ASSERT_EQ(1, secondCache->GetDiskHitCount());

		ASSERT_EQ(firstRun.files_, secondRun.files_);
	}

	//-------------------------------------------------------------------------
	TEST(DebugInformationEnumeratorTest, PdbLineTableCacheSymbolIndexes)
	{
		auto cache = std::make_shared<CppCoverage::PdbLineTableCache>();

		// A table cached without symbol indexes must not satisfy a request
		// that needs them.
		EnumerateBinary(cache, false);
		ASSERT_EQ(1, cache->GetMissCount());

		EnumerateBinary(cache, true);
		ASSERT_EQ(2, cache->GetMissCount());

		// The richer table satisfies both kinds of request.
		EnumerateBinary(cache, true);
		EnumerateBinary(cache, false);
		ASSERT_EQ(2, cache->GetMissCount());
		ASSERT_EQ(2, cache->GetMemoryHitCount());
	}
}