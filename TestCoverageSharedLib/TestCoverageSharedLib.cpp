// DllForCppCoverageTest.cpp : Defines the entry point for the application.
//

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
#include "TestCoverageSharedLib.hpp"

#include <iostream>

namespace fs = std::filesystem;

namespace TestCoverageSharedLib
{
	//-------------------------------------------------------------------------
	bool IsOdd(std::int32_t n)
	{
		if (n % 2 == 0) 
			return true;

		return false;
	}

	//-------------------------------------------------------------------------
	// These cross the DLL boundary into the test executables, which run on a
	// separate static CRT heap, so they hand back a pointer into this DLL's own
	// static storage instead of returning a heap-owning std::filesystem::path.
	// The inline GetMainCppPath / GetOutputBinaryPath wrappers (in the header)
	// rebuild a path in the caller, keeping call sites unchanged.
	const wchar_t* GetMainCppPathC()
	{
		static const std::wstring path = fs::path(__FILE__).wstring();
		return path.c_str();
	}

	//-------------------------------------------------------------------------
	const wchar_t* GetOutputBinaryPathC()
	{
		static const std::wstring path =
		    (fs::path(OUT_DIR) / TARGET_FILE_NAME).wstring();
		return path.c_str();
	}

	//-------------------------------------------------------------------------
	void CallSharedFunctionFromSharedLib()
	{
		SharedFunction(true);
	}
}