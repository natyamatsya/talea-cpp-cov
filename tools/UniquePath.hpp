// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2016 OpenCppCoverage

#pragma once

#include "ToolsExport.hpp"

#include <set>
#include <filesystem>

namespace Tools
{
	class TOOLS_DLL UniquePath
	{
	public:
		UniquePath() = default;

		UniquePath(const UniquePath&) = delete;
		UniquePath& operator=(const UniquePath&) = delete;
		UniquePath(UniquePath&&) = delete;
		UniquePath& operator=(UniquePath&&) = delete;

		// GetUniquePath returns "path" if there was no previous 
		// call to GetUniquePath with this value else it returns an unique
		// path starting with "path".
		std::filesystem::path GetUniquePath(const std::filesystem::path& path);

	private:
		std::set<std::filesystem::path> existingPathSet_;
	};
}
