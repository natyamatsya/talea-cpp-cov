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
