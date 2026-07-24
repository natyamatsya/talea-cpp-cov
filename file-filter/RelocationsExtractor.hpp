// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2016 OpenCppCoverage

#pragma once

#include <windows.h>
#include <unordered_set>
#include "FileFilterExport.hpp"
#include "IRelocationsExtractor.hpp"

namespace FileFilter
{
	class IRelocationsExtractor;

	class FILEFILTER_DLL RelocationsExtractor: public IRelocationsExtractor
	{
	public:
	  std::unordered_set<DWORD64> Extract(HANDLE hProcess,
		                                  DWORD64 baseOfImage) const;
	};
}