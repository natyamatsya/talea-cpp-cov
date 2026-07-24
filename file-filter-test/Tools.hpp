// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2016 OpenCppCoverage

#pragma once

#include <vector>
#include <string>

namespace FileFilter
{
	class File;
}

namespace FileFilterTest
{
	std::vector<FileFilter::File> ToFiles(const std::vector<std::wstring>& filenames);
}

