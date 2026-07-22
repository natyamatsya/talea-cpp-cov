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

#include <filesystem>

namespace TestCoverageOptimizedBuild
{
	// C-ABI exports + inline wrappers so nothing heap-owning crosses the DLL
	// boundary under the static CRT (see TestCoverageSharedLib for the pattern).
	__declspec(dllexport) const wchar_t* GetMainCppPathC();
	__declspec(dllexport) const wchar_t* GetOutputBinaryPathC();

	inline std::filesystem::path GetMainCppPath()
	{
		return GetMainCppPathC();
	}
	inline std::filesystem::path GetOutputBinaryPath()
	{
		return GetOutputBinaryPathC();
	}

	__declspec(dllexport) void TestOptimizedBuild();
}
