// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2014 OpenCppCoverage

#pragma once

#include <string>
#include <vector>
#include <memory>

#include "TaleaCppCovExport.hpp"

namespace TaleaCppCov
{
	class CoverageFilterSettings;
	class Patterns;

	class TALEACPPCOV_DLL WildcardCoverageFilter
	{
	public:
		explicit WildcardCoverageFilter(const CoverageFilterSettings&);
		~WildcardCoverageFilter();

		bool IsModuleSelected(const std::wstring& filename) const;
		bool IsSourceFileSelected(const std::wstring& filename) const;

	private:
		WildcardCoverageFilter(const WildcardCoverageFilter&) = delete;
		WildcardCoverageFilter& operator=(const WildcardCoverageFilter&) = delete;
		
		struct Filter;

		std::unique_ptr<Filter> BuildFilter(const Patterns& pattern) const;
		bool Match(
			const std::wstring& str,
			const Filter& filter,
			std::wostream& ostr) const;
	private:
		std::unique_ptr<Filter> moduleFilter_;
		std::unique_ptr<Filter> sourceFilter_;		
	};
}


