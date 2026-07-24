// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2014 OpenCppCoverage

#include "stdafx.h"
#include "CoverageFilterSettings.hpp"

#include "tools/Log.hpp"

namespace TaleaCppCov
{
	//-------------------------------------------------------------------------
	CoverageFilterSettings::CoverageFilterSettings(
		const Patterns& modulePatterns,
		const Patterns& sourcePatterns)
		: modulePatterns_(modulePatterns)
		, sourcePatterns_(sourcePatterns)
	{
	}

	//-------------------------------------------------------------------------
	const Patterns& CoverageFilterSettings::GetModulePatterns() const
	{
		return modulePatterns_;
	}
	
	//-------------------------------------------------------------------------
	const Patterns& CoverageFilterSettings::GetSourcePatterns() const
	{
		return sourcePatterns_;
	}

}

