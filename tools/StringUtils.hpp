// OpenCppCoverage is an open source code coverage for C++.
// Copyright (C) 2026 OpenCppCoverage
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

#pragma once

// Small string helpers replacing the boost::algorithm functions the STL does
// not provide (case-insensitive compare/search, trim, replace, split). The
// plain prefix/suffix/substring checks now use std::string's C++20/23 members
// (starts_with/ends_with/contains) directly and are not wrapped here. Case
// folding is ASCII-simple (towlower), matching how these are used on filenames
// and diff markers.

#include <algorithm>
#include <cctype>
#include <cwctype>
#include <string>
#include <string_view>
#include <vector>

namespace Tools
{
	//-------------------------------------------------------------------------
	inline char ToLowerChar(char c)
	{
		return static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
	}
	inline wchar_t ToLowerChar(wchar_t c)
	{
		return static_cast<wchar_t>(std::towlower(static_cast<wint_t>(c)));
	}

	//-------------------------------------------------------------------------
	inline bool IsSpaceChar(char c)
	{
		return std::isspace(static_cast<unsigned char>(c)) != 0;
	}
	inline bool IsSpaceChar(wchar_t c)
	{
		return std::iswspace(static_cast<wint_t>(c)) != 0;
	}

	//-------------------------------------------------------------------------
	template <class C>
	void ToLower(std::basic_string<C>& value)
	{
		std::transform(value.begin(),
		               value.end(),
		               value.begin(),
		               [](C c) { return ToLowerChar(c); });
	}

	//-------------------------------------------------------------------------
	template <class C>
	std::basic_string<C> ToLowerCopy(std::basic_string<C> value)
	{
		ToLower(value);
		return value;
	}

	//-------------------------------------------------------------------------
	template <class C>
	bool IEquals(const std::basic_string<C>& a, std::basic_string_view<C> b)
	{
		return a.size() == b.size() &&
		       std::equal(a.begin(), a.end(), b.begin(), [](C x, C y) {
			       return ToLowerChar(x) == ToLowerChar(y);
		       });
	}

	//-------------------------------------------------------------------------
	template <class C>
	bool IStartsWith(const std::basic_string<C>& value,
	                 std::basic_string_view<C> prefix)
	{
		return value.size() >= prefix.size() &&
		       std::equal(
		           prefix.begin(), prefix.end(), value.begin(), [](C x, C y) {
			           return ToLowerChar(x) == ToLowerChar(y);
		           });
	}

	//-------------------------------------------------------------------------
	template <class C, class Pred>
	void TrimIf(std::basic_string<C>& value, Pred shouldTrim)
	{
		auto keep = [&](C c) { return !shouldTrim(c); };
		value.erase(value.begin(),
		            std::find_if(value.begin(), value.end(), keep));
		value.erase(std::find_if(value.rbegin(), value.rend(), keep).base(),
		            value.end());
	}

	//-------------------------------------------------------------------------
	template <class C>
	void Trim(std::basic_string<C>& value)
	{
		TrimIf(value, [](C c) { return IsSpaceChar(c); });
	}

	//-------------------------------------------------------------------------
	template <class C>
	void ReplaceAll(std::basic_string<C>& value,
	                std::basic_string_view<C> from,
	                std::basic_string_view<C> to)
	{
		if (from.empty())
			return;
		std::size_t pos = 0;
		while ((pos = value.find(from.data(), pos, from.size())) !=
		       std::basic_string<C>::npos)
		{
			value.replace(pos, from.size(), to.data(), to.size());
			pos += to.size();
		}
	}

	//-------------------------------------------------------------------------
	template <class C>
	void ReplaceFirst(std::basic_string<C>& value,
	                  std::basic_string_view<C> from,
	                  std::basic_string_view<C> to)
	{
		auto pos = value.find(from.data(), 0, from.size());
		if (pos != std::basic_string<C>::npos)
			value.replace(pos, from.size(), to.data(), to.size());
	}

	//-------------------------------------------------------------------------
	// Case-insensitive replacement of the first occurrence (from's original
	// case in value is replaced; to is inserted verbatim).
	template <class C>
	void IReplaceFirst(std::basic_string<C>& value,
	                   std::basic_string_view<C> from,
	                   std::basic_string_view<C> to)
	{
		auto lowered = ToLowerCopy(std::basic_string<C>{value});
		auto loweredFrom = ToLowerCopy(std::basic_string<C>{from});
		auto pos = lowered.find(loweredFrom);
		if (pos != std::basic_string<C>::npos)
			value.replace(pos, from.size(), to.data(), to.size());
	}

	//-------------------------------------------------------------------------
	// Splits on every character for which isDelimiter is true, keeping empty
	// tokens between adjacent delimiters (boost::split default behavior).
	template <class C, class Pred>
	void Split(std::vector<std::basic_string<C>>& out,
	           std::basic_string_view<C> value,
	           Pred isDelimiter)
	{
		out.clear();
		std::basic_string<C> current;
		for (C c : value)
		{
			if (isDelimiter(c))
			{
				out.push_back(current);
				current.clear();
			}
			else
				current.push_back(c);
		}
		out.push_back(current);
	}
}
