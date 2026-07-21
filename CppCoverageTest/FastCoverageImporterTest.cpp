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

#include "CppCoverage/FastCoverageImporter.hpp"

namespace cov = CppCoverage;

namespace CppCoverageTest
{
	namespace
	{
		cov::FastCoverageImporter::Instantiation
		MakeInstantiation(const std::wstring& name,
		                  const std::vector<std::pair<unsigned int, unsigned int>>&
		                      lineCounts)
		{
			cov::FastCoverageImporter::Instantiation instantiation;
			instantiation.name_ = name;
			instantiation.file_ = L"foo.cpp";
			for (const auto& [line, count] : lineCounts)
				instantiation.lines_.push_back({line, count});
			return instantiation;
		}
	}

	//-------------------------------------------------------------------------
	// The whole point of the instantiation axis: a line executed for one
	// instantiation but not another must be reported as a gap, where plain
	// line coverage would show it simply covered.
	TEST(FastCoverageImporterTest, TemplateReportShowsPerInstantiationGaps)
	{
		cov::FastCoverageImporter::TemplateReport report;
		report.stem_ = L"foo";
		// Line 1 runs for both; line 2 only for <int>; line 3 only for <double>.
		report.instantiations_.push_back(
		    MakeInstantiation(L"foo<int>", {{1, 5}, {2, 5}, {3, 0}}));
		report.instantiations_.push_back(
		    MakeInstantiation(L"foo<double>", {{1, 5}, {2, 0}, {3, 5}}));

		auto markdown =
		    cov::FastCoverageImporter::FormatTemplateReport({report});

		EXPECT_NE(std::wstring::npos, markdown.find(L"## foo"));
		EXPECT_NE(std::wstring::npos, markdown.find(L"foo<int>"));
		EXPECT_NE(std::wstring::npos, markdown.find(L"foo<double>"));

		// Lines 2 and 3 are gaps (covered in one instantiation only); line 1
		// is covered in both and must not be listed.
		auto gapsPos = markdown.find(L"some but not all");
		ASSERT_NE(std::wstring::npos, gapsPos);
		auto gaps = markdown.substr(gapsPos);
		EXPECT_NE(std::wstring::npos, gaps.find(L"2"));
		EXPECT_NE(std::wstring::npos, gaps.find(L"3"));
	}

	//-------------------------------------------------------------------------
	TEST(FastCoverageImporterTest, EmptyTemplateReport)
	{
		auto markdown = cov::FastCoverageImporter::FormatTemplateReport({});
		EXPECT_NE(std::wstring::npos,
		          markdown.find(L"No template instantiations found."));
	}
}
