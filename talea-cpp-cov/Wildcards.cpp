// OpenCppCoverage is an open source code coverage for C++.
// Copyright (C) 2014 OpenCppCoverage
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

#include "stdafx.h"
#include "Wildcards.hpp"

#include "tools/StringUtils.hpp"
#include "tools/Tool.hpp"

namespace TaleaCppCov
{
	//-------------------------------------------------------------------------
	const std::vector<wchar_t> Wildcards::EscapedChars =
		{ L'\\', L'^', L'$', L'.', L'+', L'(', L')', L'[', L']', L'{', L'}', L'|', L'?' };

	namespace
	{
		//---------------------------------------------------------------------
		// Turns a wildcard expression into a UTF-8 regex: every regex
		// metacharacter is escaped and '*' becomes '.*'. Matching is done with
		// regex_search semantics (Wildcards::Match), so no anchoring is added.
		std::string BuildRegexPattern(std::wstring str)
		{
			for (auto c : Wildcards::EscapedChars)
			{
				std::wstring escapedString{ c };
				Tools::ReplaceAll(str,
				                  std::wstring_view{escapedString},
				                  std::wstring_view{L"\\" + escapedString});
			}

			// remove useless *
			Tools::TrimIf(str, [](wchar_t c){ return c == L'*';});
			Tools::ReplaceAll(str, std::wstring_view{L"*"}, std::wstring_view{L".*"});
			return Tools::ToUtf8String(str);
		}
	}

	//-------------------------------------------------------------------------
	Wildcards::Wildcards(std::wstring str, bool isRegexCaseSensitiv)
		: wildcards_{BuildRegexPattern(str),
		             Tools::Regex::Options{.caseInsensitive = !isRegexCaseSensitiv}}
		, originalStr_(str)
	{
	}

	//-------------------------------------------------------------------------
	Wildcards::Wildcards(Wildcards&& wildcards)
		: wildcards_(std::move(wildcards.wildcards_))
		, originalStr_(std::move(wildcards.originalStr_))
	{
	}

	//-------------------------------------------------------------------------
	bool Wildcards::Match(const std::wstring& str) const
	{
		return wildcards_.Matches(Tools::ToUtf8String(str));
	}

	//-------------------------------------------------------------------------
	std::wostream& operator<<(std::wostream& ostr, const Wildcards& wildcards)
	{
		ostr << wildcards.originalStr_;

		return ostr;
	}
}
