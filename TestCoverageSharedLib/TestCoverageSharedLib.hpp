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

namespace TestCoverageSharedLib
{
	// C-ABI exports: return a pointer into this DLL's static storage so nothing
	// heap-owning crosses into the test executables' separate static CRT heap.
	__declspec(dllexport) const wchar_t* GetMainCppPathC();
	__declspec(dllexport) const wchar_t* GetOutputBinaryPathC();

	// Convenience wrappers, inline so they are compiled into (and rebuild the
	// path on) the caller side; call sites use these unchanged.
	inline std::filesystem::path GetMainCppPath()
	{
		return GetMainCppPathC();
	}
	inline std::filesystem::path GetOutputBinaryPath()
	{
		return GetOutputBinaryPathC();
	}

	__declspec(dllexport) bool IsOdd(std::int32_t n);

	__declspec(dllexport) void CallSharedFunctionFromSharedLib();

	//-------------------------------------------------------------------------
	inline std::filesystem::path GetSharedFunctionFilename()
	{
		return std::filesystem::path(__FILE__).filename();
	}

	//-------------------------------------------------------------------------
	inline std::int32_t GetSharedFunctionLine()
	{
		return __LINE__ + 4;
	}

	//-------------------------------------------------------------------------
	inline bool SharedFunction(bool calledFromSharedLib)
	{
		if (calledFromSharedLib)
			return true;
		return false;
	}
}
