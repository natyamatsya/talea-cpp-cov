// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2014 OpenCppCoverage

#pragma once

#include <string>
#include <vector>

#include "TaleaCppCovExport.hpp"
#include "Patterns.hpp"

namespace TaleaCppCov
{
	class TALEACPPCOV_DLL CoverageFilterSettings
	{	
	public:
		CoverageFilterSettings(const Patterns& modulePatterns, const Patterns& sourcePatterns);
		CoverageFilterSettings(const CoverageFilterSettings&) = default;
		CoverageFilterSettings& operator=(const CoverageFilterSettings&) = delete;

		const Patterns& GetModulePatterns() const;
		const Patterns& GetSourcePatterns() const;

	private:
		Patterns modulePatterns_;
		Patterns sourcePatterns_;
	};
}


