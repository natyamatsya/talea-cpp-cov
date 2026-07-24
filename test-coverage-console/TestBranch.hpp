// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2014 OpenCppCoverage
// SPDX-FileCopyrightText: 2026 natyamatsya and talea-cpp-cov contributors

#pragma once

#include <cstdint>
#include <filesystem>

#include "SourceLineTag.hpp"

namespace TestCoverageConsole
{
	std::int32_t RunTestBranch(bool takeBranch);

	//-------------------------------------------------------------------------
	inline std::int32_t GetTestBranchConditionLine()
	{
		return GetSourceLineWithTag(
		    std::filesystem::path(__FILE__).replace_extension("cpp"),
		    "//! test-branch-condition");
	}

	//-------------------------------------------------------------------------
	inline std::filesystem::path GetTestBranchFilename()
	{
		return std::filesystem::path(__FILE__).replace_extension("cpp").filename();
	}
}
