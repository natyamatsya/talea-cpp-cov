// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2014 OpenCppCoverage
// SPDX-FileCopyrightText: 2026 natyamatsya and talea-cpp-cov contributors

#include <cstdint>

#include "TestBranch.hpp"

// GetTestBranchConditionLine() locates the "if" below by its //! tag, so this
// file is free to move; the marker travels with the line.
namespace TestCoverageConsole
{
	std::int32_t RunTestBranch(bool takeBranch)
	{
		if (takeBranch) //! test-branch-condition
			return 42;

		return 0;
	}
}
