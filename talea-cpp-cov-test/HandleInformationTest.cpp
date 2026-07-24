// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2014 OpenCppCoverage

#include "stdafx.h"

#include <filesystem>
#include <utility>

#include "talea-cpp-cov/HandleInformation.hpp"
#include "talea-cpp-cov/Handle.hpp"
#include "test-coverage-console/TestCoverageConsole.hpp"
#include "tools/Tool.hpp"

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