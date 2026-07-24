// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2016 OpenCppCoverage

#pragma once
#include <cstdint>

#include "FileFilterExport.hpp"

#include <vector>

#include <optional>
#include <filesystem>
#include "PathMatcher.hpp"

namespace FileFilter
{
	class File;

	class FILEFILTER_DLL UnifiedDiffCoverageFilter
	{
	public:
		UnifiedDiffCoverageFilter(
			const std::filesystem::path& unifiedDiffPath,
			const std::optional<std::filesystem::path>& rootDiffFolder);

		UnifiedDiffCoverageFilter(
			std::vector<File>&&,
			const std::optional<std::filesystem::path>& rootDiffFolder);

		bool IsSourceFileSelected(const std::filesystem::path&);
		bool IsLineSelected(const std::filesystem::path&, std::int32_t lineNumber);
		std::vector<std::filesystem::path> GetUnmatchedPaths() const;

	private:
		UnifiedDiffCoverageFilter(const UnifiedDiffCoverageFilter&) = delete;
		UnifiedDiffCoverageFilter& operator=(const UnifiedDiffCoverageFilter&) = delete;
		UnifiedDiffCoverageFilter(UnifiedDiffCoverageFilter&&) = delete;
		UnifiedDiffCoverageFilter& operator=(UnifiedDiffCoverageFilter&&) = delete;

		File* SearchFile(const std::filesystem::path&);

		std::filesystem::path lastPath_;
		File* lastFile_;
		PathMatcher pathMatcher_;
	};
}

