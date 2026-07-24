// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2014 OpenCppCoverage

#include "stdafx.h"
#include "HtmlFile.hpp"

namespace Exporter
{
	//-------------------------------------------------------------------------
	HtmlFile::HtmlFile(
		std::filesystem::path absolutePath,
		std::filesystem::path relativeLinkPath)
		: absolutePath_(absolutePath)
		, relativeLinkPath_(relativeLinkPath)
	{
	}

	//-------------------------------------------------------------------------
	const std::filesystem::path& HtmlFile::GetAbsolutePath() const
	{
		return absolutePath_;
	}

	//-------------------------------------------------------------------------
	const std::filesystem::path& HtmlFile::GetRelativeLinkPath() const
	{
		return relativeLinkPath_;
	}
}
