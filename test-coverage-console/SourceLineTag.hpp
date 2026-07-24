// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 natyamatsya and talea-cpp-cov contributors

#pragma once

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>

namespace TestCoverageConsole
{
	// Returns the 1-based line number of the source line in `sourcePath` whose
	// text contains `tag` (0 if none). Coverage fixtures mark a line of
	// interest with a trailing comment - e.g. `//! run-test-basic` - so the
	// tests locate it by tag instead of a hardcoded line number that any edit
	// above (an added include, a reformat) would silently shift. The marker
	// travels with the line, so the expectation stays correct for free.
	inline std::int32_t GetSourceLineWithTag(const std::filesystem::path& sourcePath,
	                                         std::string_view tag)
	{
		std::ifstream ifs(sourcePath);
		std::int32_t number = 0;
		for (std::string line; std::getline(ifs, line);)
		{
			++number;
			if (line.find(tag) != std::string::npos)
				return number;
		}
		return 0;
	}
}
