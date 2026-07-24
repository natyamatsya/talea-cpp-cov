// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2016 OpenCppCoverage

#include "stdafx.h"
#include <cstdint>
#include "UniquePath.hpp"

#include <format>
#include <string>
#include <filesystem>

namespace Tools
{
	//-------------------------------------------------------------------------
	std::filesystem::path UniquePath::GetUniquePath(const std::filesystem::path& path)
	{
		auto uniquePath = path;
		auto filenameWithoutExtension = path.filename().replace_extension(L"");

		for (std::int32_t i = 2; existingPathSet_.count(uniquePath) != 0; ++i)
		{
			auto filename = std::format(L"{}{}{}",
			                            filenameWithoutExtension.wstring(),
			                            i,
			                            path.extension().wstring());
			uniquePath = path.parent_path() / filename;
		}
		existingPathSet_.insert(uniquePath);
		return uniquePath;
	}
}
