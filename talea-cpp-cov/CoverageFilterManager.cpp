// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2016 OpenCppCoverage

#include "stdafx.h"


#include "CoverageFilterManager.hpp"
#include "UnifiedDiffCoverageFilterManager.hpp"

#include "file-filter/FileInfo.hpp"
#include "file-filter/LineInfo.hpp"
#include "file-filter/ReleaseCoverageFilter.hpp"

namespace TaleaCppCov
{
	//-------------------------------------------------------------------------
	CoverageFilterManager::CoverageFilterManager(
		const CoverageFilterSettings& settings,
		const std::vector<UnifiedDiffSettings>& unifiedDiffSettingsCollection,
		const std::vector<std::wstring>& excludedLineRegexes,
		bool useReleaseCoverageFilter)
		: wildcardCoverageFilter_{ settings }
		, unifiedDiffCoverageFilterManager_{ unifiedDiffSettingsCollection }
		, lineFilter_{ excludedLineRegexes }
		, optionalReleaseCoverageFilter_{ useReleaseCoverageFilter ?
			std::make_unique<FileFilter::ReleaseCoverageFilter>() : nullptr }		
	{
	}

	//-------------------------------------------------------------------------
	CoverageFilterManager::~CoverageFilterManager() = default;

	//-------------------------------------------------------------------------
	bool CoverageFilterManager::IsModuleSelected(const std::wstring& filename) const
	{
		return wildcardCoverageFilter_.IsModuleSelected(filename);
	}

	//-------------------------------------------------------------------------
	bool CoverageFilterManager::IsSourceFileSelected(const std::wstring& filename)
	{
		if (!wildcardCoverageFilter_.IsSourceFileSelected(filename))
			return false;

		return unifiedDiffCoverageFilterManager_.IsSourceFileSelected(filename);
	}

	//-------------------------------------------------------------------------
	bool CoverageFilterManager::IsLineSelected(
		const FileFilter::ModuleInfo& moduleInfo,
		const FileFilter::FileInfo& fileInfo,
		const FileFilter::LineInfo& lineInfo)
	{
		if (optionalReleaseCoverageFilter_ &&
			!optionalReleaseCoverageFilter_->IsLineSelected(moduleInfo, fileInfo, lineInfo))
		{
			return false;
		}

		if (!lineFilter_.IsLineSelected(fileInfo, lineInfo))
			return false;

		return unifiedDiffCoverageFilterManager_.IsLineSelected(fileInfo, lineInfo);
	}

	//-------------------------------------------------------------------------
	std::vector<std::wstring> CoverageFilterManager::ComputeWarningMessageLines(size_t maxUnmatchPaths) const
	{
		return unifiedDiffCoverageFilterManager_.ComputeWarningMessageLines(maxUnmatchPaths);
	}
}
