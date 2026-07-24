// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 natyamatsya and talea-cpp-cov contributors

#include "stdafx.h"

#include "talea-cpp-cov/ImportClosure.hpp"
#include "test-coverage-console/TestCoverageConsole.hpp"
#include "test-coverage-shared-lib/TestCoverageSharedLib.hpp"

#include "coverage/CoverageData.hpp"
#include "test-helper/CoverageDataComparer.hpp"
#include "TestTools.hpp"

namespace TaleaCppCovTest
{
	namespace
	{
		bool ContainsFilename(
		    const std::vector<std::filesystem::path>& paths,
		    const std::wstring& filename)
		{
			for (const auto& path : paths)
			{
				if (_wcsicmp(path.filename().wstring().c_str(),
				             filename.c_str()) == 0)
					return true;
			}
			return false;
		}
	}

	//-------------------------------------------------------------------------
	// TestCoverageConsole links TestCoverageSharedLib, so the shared library is
	// in its static-import closure and lives next to the executable.
	TEST(ImportClosureTest, IncludesSharedLibDll)
	{
		auto closure = TaleaCppCov::ComputeImportClosure(
		    TestCoverageConsole::GetOutputBinaryPath());

		auto sharedLib =
		    TestCoverageSharedLib::GetOutputBinaryPath().filename().wstring();
		ASSERT_TRUE(ContainsFilename(closure, sharedLib));
	}

	//-------------------------------------------------------------------------
	// Every returned path exists on disk and the executable itself is excluded.
	TEST(ImportClosureTest, ReturnsExistingModulesWithoutTheExecutable)
	{
		auto exePath = TestCoverageConsole::GetOutputBinaryPath();
		auto closure = TaleaCppCov::ComputeImportClosure(exePath);

		for (const auto& module : closure)
		{
			ASSERT_TRUE(std::filesystem::exists(module));
			ASSERT_STRNE(module.filename().string().c_str(),
			             exePath.filename().string().c_str());
		}
	}

	//-------------------------------------------------------------------------
	// Pre-parsing the import closure only warms the cache: coverage of a run
	// that loads a DLL must be identical with the option on and off.
	TEST(ImportClosureTest, PreparseImportsMatchesNormalRun)
	{
		TestTools::CoverageArgs args{
		    {TestCoverageConsole::TestSharedLib},
		    TestCoverageSharedLib::GetOutputBinaryPath().wstring(),
		    TestCoverageSharedLib::GetMainCppPath().wstring()};
		args.coverChildren_ = false;

		auto normal = TestTools::ComputeCoverageDataPatterns(args);
		args.preparseImports_ = true;
		auto preparsed = TestTools::ComputeCoverageDataPatterns(args);

		TestHelper::CoverageDataComparer().AssertEquals(normal, preparsed);
	}
}
