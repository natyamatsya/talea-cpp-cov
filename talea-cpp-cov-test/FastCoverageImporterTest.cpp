// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 natyamatsya and talea-cpp-cov contributors

#include "stdafx.h"
#include <cstdint>

#include <windows.h>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "talea-cpp-cov/FastCoverageImporter.hpp"
#include "talea-cpp-cov/CoverageFilterSettings.hpp"
#include "talea-cpp-cov/Patterns.hpp"
#include "coverage-runtime/FastCovFormat.hpp"
#include "coverage/CoverageData.hpp"
#include "test-helper/CoverageDataComparer.hpp"
#include "test-helper/TemporaryPath.hpp"
#include "test-helper/Tools.hpp"
#include "TestTools.hpp"

namespace cov = TaleaCppCov;
namespace fs = std::filesystem;

namespace TaleaCppCovTest
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

		// Compare line coverage exactly. Branch (condition) coverage is
		// deliberately excluded: fast-mode derives it from block counters,
		// which is exact at sole-predecessor successors but approximate at
		// control-flow merges, so it cannot match the debugger's per-Jcc
		// observation edge-for-edge. Line coverage is block-accurate and must
		// match exactly.
		TestHelper::CoverageDataComparer().AssertEquals(
		    debuggerCoverage, fastCovCoverage, /*compareBranches*/ false);
	}

	namespace
	{
		cov::CoverageFilterSettings FastCovFilter(const std::wstring& modulePattern,
		                                          const std::wstring& sourcePattern)
		{
			cov::Patterns modules{false};
			modules.AddSelectedPatterns(modulePattern);
			cov::Patterns sources{false};
			sources.AddSelectedPatterns(sourcePattern);
			return cov::CoverageFilterSettings{modules, sources};
		}

		// Runs program with the dump output pointed at output (a folder or a
		// file) and returns its exit code.
		std::int32_t RunWithDumpOutput(const fs::path& program,
		                               const std::vector<std::string>& args,
		                               const fs::path& output)
		{
			::SetEnvironmentVariableW(FastCov::OutputPathEnvVar,
			                          output.wstring().c_str());
			auto result = TestHelper::LaunchProcess(program, args, "", false);
			::SetEnvironmentVariableW(FastCov::OutputPathEnvVar, nullptr);
			return result.exitCode;
		}

		std::vector<fs::path> ListDumps(const fs::path& folder)
		{
			std::vector<fs::path> dumps;
			for (const auto& entry : fs::directory_iterator{folder})
				if (entry.path().extension() == L".fastcov")
					dumps.push_back(entry.path());
			return dumps;
		}

		Coverage::CoverageData ImportDump(const fs::path& dump,
		                                  const std::wstring& modulePattern,
		                                  const std::wstring& sourcePattern)
		{
			auto filter = FastCovFilter(modulePattern, sourcePattern);
			return cov::FastCoverageImporter{filter, {}, {}, {}}
			    .Import(dump)
			    .value()
			    .coverageData_;
		}

		Coverage::CoverageData DebuggerCoverage(
		    const fs::path& program,
		    const std::vector<std::wstring>& arguments,
		    const std::wstring& modulePattern,
		    const std::wstring& sourcePattern)
		{
			TestTools::CoverageArgs args{arguments, modulePattern, sourcePattern};
			args.programToRun_ = program;
			return TestTools::ComputeCoverageDataPatterns(args);
		}

		std::vector<char> ReadAllBytes(const fs::path& path)
		{
			std::ifstream file{path, std::ios::binary};
			return {std::istreambuf_iterator<char>{file}, {}};
		}

		bool LongPathsEnabled()
		{
			DWORD value = 0;
			DWORD size = sizeof(value);
			return ::RegGetValueW(HKEY_LOCAL_MACHINE,
			                      L"SYSTEM\\CurrentControlSet\\Control\\FileSystem",
			                      L"LongPathsEnabled",
			                      RRF_RT_REG_DWORD,
			                      nullptr,
			                      &value,
			                      &size) == ERROR_SUCCESS &&
			       value != 0;
		}

		const std::wstring DllModule = L"TestFastCovDll.dll";
		const std::wstring DllSource = L"TestFastCovDll.cpp";

		// Compares the DLL's coverage from a dump with the debugger's. The
		// debugger names its run after the host it launched, the import after
		// the dump's first module; only the coverage itself must match.
		void AssertDllCoverageMatchesDebugger(const fs::path& host,
		                                      const std::wstring& argument,
		                                      const fs::path& dump)
		{
			auto debugger = DebuggerCoverage(host, {argument}, DllModule, DllSource);
			auto fastCov = ImportDump(dump, DllModule, DllSource);
			ASSERT_EQ(1u, fastCov.GetModules().size());
			fastCov.SetName(debugger.GetName());
			TestHelper::CoverageDataComparer().AssertEquals(
			    debugger, fastCov, /*compareBranches*/ false);
		}
	}

	//-------------------------------------------------------------------------
	// A DLL freed before the process exits is gone by the time the executable's
	// exit dump enumerates the modules, so the DLL's own runtime copy dumps it
	// at unload. The host itself carries no counters and writes nothing.
	TEST(FastCoverageImporterTest, DllUnloadedBeforeExitWritesItsOwnDump)
	{
		const auto host = fs::path{OUT_DIR} / L"TestFastCovDllHost.exe";
		if (!fs::exists(host))
			GTEST_SKIP() << "TestFastCovDllHost fixture is MSVC-only.";

		TestHelper::TemporaryPath folder{
		    TestHelper::TemporaryPathOption::CreateAsFolder};
		ASSERT_EQ(0, RunWithDumpOutput(host, {"unload"}, folder));

		auto dumps = ListDumps(folder);
		ASSERT_EQ(1u, dumps.size());
		EXPECT_NE(std::wstring::npos,
		          dumps[0].filename().wstring().find(L"_TestFastCovDll_"));

		AssertDllCoverageMatchesDebugger(host, L"unload", dumps[0]);
	}

	//-------------------------------------------------------------------------
	// A DLL still loaded at exit is in the executable's exit dump, and its own
	// runtime copy must not dump it a second time.
	TEST(FastCoverageImporterTest, DllLoadedAtExitIsDumpedOnce)
	{
		const auto host = fs::path{OUT_DIR} / L"TestFastCovDllHost.exe";
		if (!fs::exists(host))
			GTEST_SKIP() << "TestFastCovDllHost fixture is MSVC-only.";

		TestHelper::TemporaryPath folder{
		    TestHelper::TemporaryPathOption::CreateAsFolder};
		ASSERT_EQ(0, RunWithDumpOutput(host, {"keep"}, folder));

		auto dumps = ListDumps(folder);
		ASSERT_EQ(1u, dumps.size());
		EXPECT_EQ(std::wstring::npos,
		          dumps[0].filename().wstring().find(L"_TestFastCovDll_"));

		AssertDllCoverageMatchesDebugger(host, L"keep", dumps[0]);
	}

	//-------------------------------------------------------------------------
	// The dump folder may be deeper than MAX_PATH (CI workspaces): the runtime
	// still writes the per-process dump and the importer reads it.
	TEST(FastCoverageImporterTest, DumpFolderBeyondMaxPath)
	{
		const auto fixture = fs::path{OUT_DIR} / L"TestFastCovConsole.exe";
		if (!fs::exists(fixture))
			GTEST_SKIP() << "TestFastCovConsole fixture is MSVC-only.";
		if (!LongPathsEnabled())
			GTEST_SKIP() << "Long paths are disabled on this machine.";

		TestHelper::TemporaryPath root{
		    TestHelper::TemporaryPathOption::CreateAsFolder};
		auto folder = root.GetPath();
		while (folder.wstring().size() <= 300)
			folder /= std::wstring(40, L'd');
		fs::create_directories(folder);

		ASSERT_EQ(0, RunWithDumpOutput(fixture, {}, folder));

		auto dumps = ListDumps(folder);
		ASSERT_EQ(1u, dumps.size());

		const std::wstring pattern = L"TestFastCovConsole";
		TestHelper::CoverageDataComparer().AssertEquals(
		    DebuggerCoverage(fixture, {}, pattern, pattern),
		    ImportDump(dumps[0], pattern, pattern),
		    /*compareBranches*/ false);
	}

	//-------------------------------------------------------------------------
	// The runtime writes version 2 raw dumps, which store the module path in
	// full; the importer still reads version 1 dumps (fixed-size paths) to the
	// same coverage.
	TEST(FastCoverageImporterTest, RawDumpVersions)
	{
		const auto fixture = fs::path{OUT_DIR} / L"TestFastCovConsole.exe";
		if (!fs::exists(fixture))
			GTEST_SKIP() << "TestFastCovConsole fixture is MSVC-only.";

		TestHelper::TemporaryPath dumpPath;
		ASSERT_EQ(0, RunWithDumpOutput(fixture, {}, dumpPath));
		const auto bytes = ReadAllBytes(dumpPath);

		ASSERT_GE(bytes.size(),
		          sizeof(FastCov::RawDumpHeader) +
		              sizeof(FastCov::RawModuleHeaderV2));
		FastCov::RawDumpHeader header{};
		std::memcpy(&header, bytes.data(), sizeof(header));
		EXPECT_EQ(FastCov::RawDumpMagic, header.magic_);
		EXPECT_EQ(FastCov::RawDumpVersion2, header.version_);
		ASSERT_EQ(1u, header.moduleCount_);

		FastCov::RawModuleHeaderV2 moduleHeader{};
		std::memcpy(&moduleHeader,
		            bytes.data() + sizeof(header),
		            sizeof(moduleHeader));
		const auto pathOffset = sizeof(header) + sizeof(moduleHeader);
		const auto pathBytes = moduleHeader.pathChars_ * sizeof(wchar_t);
		ASSERT_EQ(pathOffset + pathBytes + moduleHeader.regionSize_,
		          bytes.size());
		std::wstring modulePath(moduleHeader.pathChars_, L'\0');
		std::memcpy(modulePath.data(), bytes.data() + pathOffset, pathBytes);
		EXPECT_TRUE(fs::equivalent(fixture, modulePath));

		// The same dump in the version 1 layout.
		FastCov::RawDumpHeader v1Header = header;
		v1Header.version_ = FastCov::DumpVersion;
		FastCov::RawModuleHeader v1Module{};
		modulePath.copy(v1Module.path_, FastCov::MaxModulePathChars - 1);
		v1Module.regionRva_ = moduleHeader.regionRva_;
		v1Module.regionSize_ = moduleHeader.regionSize_;

		TestHelper::TemporaryPath v1Path;
		{
			std::ofstream v1{v1Path.GetPath(), std::ios::binary};
			v1.write(reinterpret_cast<const char*>(&v1Header), sizeof(v1Header));
			v1.write(reinterpret_cast<const char*>(&v1Module), sizeof(v1Module));
			v1.write(bytes.data() + pathOffset + pathBytes,
			         moduleHeader.regionSize_);
		}

		const std::wstring pattern = L"TestFastCovConsole";
		TestHelper::CoverageDataComparer().AssertEquals(
		    ImportDump(dumpPath, pattern, pattern),
		    ImportDump(v1Path, pattern, pattern));
	}
}
