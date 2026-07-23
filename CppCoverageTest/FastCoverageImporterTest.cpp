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
#include <cstdint>

#include <windows.h>
#include <filesystem>

#include "CppCoverage/FastCoverageImporter.hpp"
#include "CppCoverage/CoverageFilterSettings.hpp"
#include "CppCoverage/Patterns.hpp"
#include "CoverageRuntime/FastCovFormat.hpp"
#include "Coverage/CoverageData.hpp"
#include "TestHelper/CoverageDataComparer.hpp"
#include "TestHelper/TemporaryPath.hpp"
#include "TestHelper/Tools.hpp"
#include "TestTools.hpp"

namespace cov = CppCoverage;
namespace fs = std::filesystem;

namespace CppCoverageTest
{
	namespace
	{
		cov::FastCoverageImporter::Instantiation
		MakeInstantiation(const std::wstring& name,
		                  const std::vector<std::pair<std::uint32_t, std::uint32_t>>&
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
		// Line 1 runs for both; line 2 only for <std::int32_t>; line 3 only for <double>.
		report.instantiations_.push_back(
		    MakeInstantiation(L"foo<std::int32_t>", {{1, 5}, {2, 5}, {3, 0}}));
		report.instantiations_.push_back(
		    MakeInstantiation(L"foo<double>", {{1, 5}, {2, 0}, {3, 5}}));

		auto markdown =
		    cov::FastCoverageImporter::FormatTemplateReport({report});

		EXPECT_NE(std::wstring::npos, markdown.find(L"## foo"));
		EXPECT_NE(std::wstring::npos, markdown.find(L"foo<std::int32_t>"));
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

	namespace
	{
		cov::CoverageFilterSettings FastCovFilter(const std::wstring& pattern)
		{
			cov::Patterns modules{false};
			modules.AddSelectedPatterns(pattern);
			cov::Patterns sources{false};
			sources.AddSelectedPatterns(pattern);
			return cov::CoverageFilterSettings{modules, sources};
		}
	}

	//-------------------------------------------------------------------------
	// End-to-end parity for the compiler-instrumentation ("fast mode") path:
	// running the instrumented fixture writes a dump, and importing it must
	// yield exactly the debugger's coverage of the same binary - the fast
	// mode is a different collection mechanism, not a different measurement.
	//
	// The fixture (TestFastCovConsole) is built only under MSVC - the clang-cl
	// fast-mode runtime needs lld, which this build does not use - so the test
	// skips when the fixture is absent.
	TEST(FastCoverageImporterTest, MatchesDebugger)
	{
		const auto fixture = fs::path{OUT_DIR} / L"TestFastCovConsole.exe";
		if (!fs::exists(fixture))
			GTEST_SKIP() << "TestFastCovConsole fixture is MSVC-only.";

		const std::wstring pattern = L"TestFastCovConsole";

		// Run the fixture with the dump path in the environment; the child
		// inherits it and the runtime writes the dump at exit.
		TestHelper::TemporaryPath dumpPath;
		ASSERT_NE(0,
		          ::SetEnvironmentVariableW(FastCov::OutputPathEnvVar,
		                                    dumpPath->wstring().c_str()));
		TestHelper::LaunchProcess(fixture, {}, "", false);
		::SetEnvironmentVariableW(FastCov::OutputPathEnvVar, nullptr);
		ASSERT_TRUE(fs::exists(dumpPath.GetPath()));

		auto filter = FastCovFilter(pattern);
		auto fastCovCoverage =
		    cov::FastCoverageImporter{filter, {}, {}, {}}
		        .Import(dumpPath.GetPath())
		        .value()
		        .coverageData_;

		// The debugger over the same fixture, restricted to the same module
		// and source, is the oracle.
		TestTools::CoverageArgs args{{}, pattern, pattern};
		args.programToRun_ = fixture;
		auto debuggerCoverage = TestTools::ComputeCoverageDataPatterns(args);

		TestHelper::CoverageDataComparer().AssertEquals(debuggerCoverage,
		                                                fastCovCoverage);
	}
}
