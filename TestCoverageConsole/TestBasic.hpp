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

#pragma once

#include <cstdint>
#include <filesystem>
#include <tchar.h>

#include "SourceLineTag.hpp"

namespace TestCoverageConsole
{
	void RunTestBasic();
	void RunChildProcesses(int argc, _TCHAR* argv[]);
	void RunNestedChildProcess();
	void UnloadReloadDll();

	//-------------------------------------------------------------------------
	inline std::int32_t GetTestBasicLine()
	{
		return GetSourceLineWithTag(
		    std::filesystem::path(__FILE__).replace_extension("cpp"),
		    "//! run-test-basic");
	}

	//-------------------------------------------------------------------------
	inline std::filesystem::path GetTestBasicFilename()
	{
		return std::filesystem::path(__FILE__).replace_extension("cpp").filename();
	}
}
