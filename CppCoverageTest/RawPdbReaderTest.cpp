// OpenCppCoverage is an open source code coverage for C++.
// Copyright (C) 2026 OpenCppCoverage
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

#include <map>
#include <set>
#include <tuple>

#include "CppCoverage/RawPdbReader.hpp"
#include "CppCoverage/PdbLineTableCache.hpp"
#include "CppCoverage/DebugInformationEnumerator.hpp"
#include "TestCoverageConsole/TestCoverageConsole.hpp"

namespace cov = CppCoverage;

namespace CppCoverageTest
{
	namespace
	{
		// Collects the DIA line table via the enumerator's OnSourceFile /
		// OnModuleFunctions callbacks, selecting every source file.
		struct DiaCollector : cov::IDebugInformationHandler
		{
			bool IsSourceFileSelected(const std::filesystem::path&) override
			{
				return true;
			}
			void OnModuleFunctions(
			    const std::vector<FunctionRange>& functions) override
			{
				functions_ = functions;
			}
			void OnSourceFile(const std::filesystem::path& path,
			                  const std::vector<Line>& lines) override
			{
				for (const auto& line : lines)
					lines_.insert(std::make_tuple(path.wstring(),
					                              line.lineNumber_,
					                              line.virtualAddress_,
					                              line.length_));
			}

			std::set<std::tuple<std::wstring, unsigned long, int64_t,
			                    unsigned long>>
			    lines_;
			std::vector<FunctionRange> functions_;
		};

		//---------------------------------------------------------------------
		std::set<std::tuple<std::wstring, unsigned long, int64_t,
		                    unsigned long>>
		FlattenRawPdb(const cov::ModuleLineTable& table)
		{
			std::set<std::tuple<std::wstring, unsigned long, int64_t,
			                    unsigned long>>
			    lines;
			for (const auto& file : table.files_)
				for (const auto& line : file.lines_)
					lines.insert(std::make_tuple(file.rawName_,
					                             line.lineNumber_,
					                             line.virtualAddress_,
					                             line.length_));
			return lines;
		}
	}

	//-------------------------------------------------------------------------
	// The raw_pdb reader must produce the same line records as DIA (raw source
	// name, line number, RVA, code length), or coverage would change.
	TEST(RawPdbReaderTest, MatchesDiaLineRecords)
	{
		auto binary = TestCoverageConsole::GetOutputBinaryPath();

		cov::ModuleLineTable rawTable;
		ASSERT_TRUE(cov::RawPdbReader::TryRead(binary, rawTable));
		auto rawLines = FlattenRawPdb(rawTable);
		ASSERT_FALSE(rawLines.empty());

		cov::DebugInformationEnumerator enumerator{{}};
		DiaCollector dia;
		ASSERT_TRUE(enumerator.Enumerate(binary, dia));

		EXPECT_EQ(dia.lines_, rawLines);
	}

	//-------------------------------------------------------------------------
	// Function ranges (address, length) must match DIA's, since run merging
	// relies on them.
	TEST(RawPdbReaderTest, MatchesDiaFunctionRanges)
	{
		auto binary = TestCoverageConsole::GetOutputBinaryPath();

		cov::ModuleLineTable rawTable;
		ASSERT_TRUE(cov::RawPdbReader::TryRead(binary, rawTable));

		cov::DebugInformationEnumerator enumerator{{}};
		DiaCollector dia;
		ASSERT_TRUE(enumerator.Enumerate(binary, dia));

		std::set<std::pair<int64_t, unsigned long>> rawRanges;
		for (const auto& f : rawTable.functions_)
			rawRanges.insert({f.virtualAddress_, f.length_});
		std::set<std::pair<int64_t, unsigned long>> diaRanges;
		for (const auto& f : dia.functions_)
			diaRanges.insert({f.virtualAddress_, f.length_});

		EXPECT_EQ(diaRanges, rawRanges);
	}
}
