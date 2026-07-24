// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2016 OpenCppCoverage

#include "stdafx.h"
#include "UnifiedDiffSettings.hpp"

namespace TaleaCppCov
{
	//-------------------------------------------------------------------------
	UnifiedDiffSettings::UnifiedDiffSettings(
		const std::filesystem::path& unifiedDiffPath,
		const std::optional<std::filesystem::path>& rootDiffFolder)
		: unifiedDiffPath_{unifiedDiffPath}
		, rootDiffFolder_{rootDiffFolder}
	{
	}

	//-------------------------------------------------------------------------
	const std::filesystem::path& UnifiedDiffSettings::GetUnifiedDiffPath() const
	{
		return unifiedDiffPath_;
	}

	//-------------------------------------------------------------------------
	const std::optional<std::filesystem::path>& UnifiedDiffSettings::GetRootDiffFolder() const
	{
		return rootDiffFolder_;
	}
}
