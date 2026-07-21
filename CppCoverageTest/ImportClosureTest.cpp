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

#include "CppCoverage/ImportClosure.hpp"
#include "TestCoverageConsole/TestCoverageConsole.hpp"
#include "TestCoverageSharedLib/TestCoverageSharedLib.hpp"

#include "Plugin/Exporter/CoverageData.hpp"
#include "TestHelper/CoverageDataComparer.hpp"
#include "TestTools.hpp"

namespace CppCoverageTest
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
		auto closure = CppCoverage::ComputeImportClosure(
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
		auto closure = CppCoverage::ComputeImportClosure(exePath);

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
