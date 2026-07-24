// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2016 OpenCppCoverage

#pragma once
#include <cstdint>

#include <vector>
#include <memory>
#include <set>

#include <filesystem>

#include "TaleaCppCovExport.hpp"

namespace FileFilter
{
	class UnifiedDiffCoverageFilter;
	class FileInfo;
	class LineInfo;
}

namespace TaleaCppCov
{
	class UnifiedDiffSettings;

	class TALEACPPCOV_DLL UnifiedDiffCoverageFilterManager
	{
	public:
		explicit UnifiedDiffCoverageFilterManager(
			const std::vector<UnifiedDiffSettings>&);

		using UnifiedDiffCoverageFilters = std::vector<std::unique_ptr<FileFilter::UnifiedDiffCoverageFilter>>;

		explicit UnifiedDiffCoverageFilterManager(
			UnifiedDiffCoverageFilters&&);

		~UnifiedDiffCoverageFilterManager();

		bool IsSourceFileSelected(const std::wstring& filename);
		bool IsLineSelected(
			const FileFilter::FileInfo&,
			const FileFilter::LineInfo&);

		std::vector<std::wstring> ComputeWarningMessageLines(size_t maxUnmatchPaths) const;

	private:
		UnifiedDiffCoverageFilterManager(const UnifiedDiffCoverageFilterManager&) = delete;
		UnifiedDiffCoverageFilterManager& operator=(const UnifiedDiffCoverageFilterManager&) = delete;

		std::vector<std::wstring> ComputeWarningMessageLines(
			const std::set<std::filesystem::path>& unmatchPaths,
			size_t maxUnmatchPaths) const;

		const std::set<std::int32_t>& GetExecutableLinesSet(const FileFilter::FileInfo&);
		const UnifiedDiffCoverageFilters unifiedDiffCoverageFilters_;

		struct ExecutableLineCache
		{
			std::filesystem::path currentFilePath;
			std::set<std::int32_t> executableLinesSet;
		};

		ExecutableLineCache executableLineCache_;
	};
}
