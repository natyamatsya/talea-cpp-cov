// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2016 OpenCppCoverage

#pragma once

#include <memory>

#include "TaleaCppCovExport.hpp"
#include "WildcardCoverageFilter.hpp"
#include "ICoverageFilterManager.hpp"
#include "UnifiedDiffCoverageFilterManager.hpp"
#include "file-filter/LineFilter.hpp"

namespace FileFilter
{
	class ReleaseCoverageFilter;
}

namespace TaleaCppCov
{
	class CoverageFilterSettings;
	class UnifiedDiffSettings;

	class TALEACPPCOV_DLL CoverageFilterManager: public ICoverageFilterManager
	{
	public:
		explicit CoverageFilterManager(
			const CoverageFilterSettings&,
			const std::vector<UnifiedDiffSettings>&,
			const std::vector<std::wstring>& excludedLineRegexes,
			bool useReleaseCoverageFilter);

		~CoverageFilterManager();

		bool IsModuleSelected(const std::wstring& filename) const override;
		bool IsSourceFileSelected(const std::wstring& filename) override;
		bool IsLineSelected(
			const FileFilter::ModuleInfo&,
			const FileFilter::FileInfo&,
			const FileFilter::LineInfo&) override;

		std::vector<std::wstring> ComputeWarningMessageLines(size_t maxUnmatchPaths) const;

	private:
		CoverageFilterManager(const CoverageFilterManager&) = delete;
		CoverageFilterManager& operator=(const CoverageFilterManager&) = delete;

		const WildcardCoverageFilter wildcardCoverageFilter_;
		UnifiedDiffCoverageFilterManager unifiedDiffCoverageFilterManager_;
		FileFilter::LineFilter lineFilter_;

		const std::unique_ptr<FileFilter::ReleaseCoverageFilter> optionalReleaseCoverageFilter_;
	};
}
