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

#include <filesystem>
#include <utility>

#include "TaleaCppCov/HandleInformation.hpp"
#include "TaleaCppCov/Handle.hpp"
#include "TestCoverageConsole/TestCoverageConsole.hpp"
#include "Tools/Tool.hpp"

#include "TestTools.hpp"

namespace cov = TaleaCppCov;
namespace fs = std::filesystem;

namespace TaleaCppCovTest
{
	namespace
	{
		// A releaser must be a function pointer (Handle::Create tests it with
		// !releaser), so the release count lives in a file-static.
		int g_handleReleaseCount = 0;
		bool CountingReleaser(int*)
		{
			++g_handleReleaseCount;
			return true;
		}
	}

	//-------------------------------------------------------------------------
	TEST(HandleInformationTest, ComputeFilename)
	{
		auto outputBinaryPath = TestCoverageConsole::GetOutputBinaryPath();
		fs::path path;

		TestTools::GetHandles(outputBinaryPath, [&](HANDLE hProcess, HANDLE hFile)
		{
			cov::HandleInformation handleInformation;

			path = Tools::ValueOrThrow(handleInformation.ComputeFilename(hFile));
		});

		ASSERT_EQ(outputBinaryPath, path);
	}

	//-------------------------------------------------------------------------
	// Regression: CreateHandle now returns the Handle inside a Tools::Expected,
	// so it is genuinely moved (the by-value factory used to elide the move).
	// A moved-from Handle must not run its releaser, or the surviving Handle's
	// resource is released early / twice.
	TEST(HandleTest, MovedFromHandleReleasesExactlyOnce)
	{
		int dummy = 0;
		g_handleReleaseCount = 0;

		{
			auto handle = cov::CreateHandle(&dummy, &CountingReleaser);
			ASSERT_TRUE(handle);
			// Move out of the Expected and back, as the factory return does.
			auto moved = std::move(*handle);
			ASSERT_EQ(&dummy, moved.GetValue());
		}

		ASSERT_EQ(1, g_handleReleaseCount);
	}
}