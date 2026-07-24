// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2016 OpenCppCoverage

#include "stdafx.h"
#include "file-filter/File.hpp"

namespace FileFilterTest
{
	//-----------------------------------------------------------------
	std::vector<FileFilter::File> ToFiles(const std::vector<std::wstring>& filenames)
	{
		std::vector<FileFilter::File> files;

		for (auto& filename : filenames)
			files.emplace_back(filename);

		return files;
	}
}