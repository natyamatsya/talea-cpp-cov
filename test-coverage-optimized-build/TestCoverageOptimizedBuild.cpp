// OpenCppCoverage is an open source code coverage for C++.
// Copyright (C) 2017 OpenCppCoverage
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

#include "TestCoverageOptimizedBuild.hpp"

#include <cstdint>
#include <memory>

namespace TestCoverageOptimizedBuild
{
	//-------------------------------------------------------------------------
	// C-ABI across the DLL boundary: return a pointer into this DLL's static
	// storage, not a heap-owning path, so nothing crosses into the test
	// executables' separate static CRT heap (see TestCoverageSharedLib).
	const wchar_t* GetMainCppPathC()
	{
		static const std::wstring path =
		    std::filesystem::path(__FILE__).wstring();
		return path.c_str();
	}

	//-------------------------------------------------------------------------
	const wchar_t* GetOutputBinaryPathC()
	{
		static const std::wstring path =
		    (std::filesystem::path(OUT_DIR) / TARGET_FILE_NAME).wstring();
		return path.c_str();
	}

	//-------------------------------------------------------------------------
	class CrashInOptimizedBuild
	{
	public:
		explicit CrashInOptimizedBuild(std::int32_t type)
		{
			switch (type)
			{
			case 0: x = 1; break;
			case 1: x = 2; break;
			case 2: x = 3; break;
			case 3: x = 4; break;
			default: throw std::runtime_error("Error");
			};
		}

		std::int32_t x;
	};

	//-------------------------------------------------------------------------
	void TestOptimizedBuild()
	{
		auto crashInOptimizedBuild = std::make_unique<CrashInOptimizedBuild>(0);
	}
}