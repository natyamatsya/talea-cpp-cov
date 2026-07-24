// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2016 OpenCppCoverage

#pragma once

#include <windows.h>
#include <vector>
#include <filesystem>

#include "LineInfo.hpp"

namespace FileFilter
{
	class FileInfo
	{
	public:
		FileInfo(
			const std::filesystem::path& filePath,
			std::vector<LineInfo>&& lineInfoColllection)
			: filePath_{ filePath }
			, lineInfoColllection_{ std::move(lineInfoColllection) }
		{}

		const std::filesystem::path filePath_;
		const std::vector<LineInfo> lineInfoColllection_;
	};	
}
