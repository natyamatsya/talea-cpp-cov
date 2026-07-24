// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2016 OpenCppCoverage

#pragma once

#include <windows.h>
#include <unordered_set>
#include "FileFilterExport.hpp"

namespace FileFilter
{
	class FILEFILTER_DLL IRelocationsExtractor
	{
	public:
		virtual ~IRelocationsExtractor() = default;
		virtual std::unordered_set<DWORD64>
		Extract(HANDLE hProcess, DWORD64 baseOfImage) const = 0;
	};
}