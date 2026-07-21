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

#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "ToolsExport.hpp"

namespace Tools
{
	// A thin, move-only RAII wrapper around a compiled PCRE2 pattern.
	//
	// PCRE2 is a byte/UTF-8 engine, so every pattern and subject passed here is
	// a std::string. Callers holding std::wstring convert at the boundary with
	// Tools::ToUtf8String (Unicode text) or Tools::ToLocalString (raw file
	// bytes). The pcre2.h header is kept out of this file so only Tools.dll
	// needs to see it.
	//
	// Compared to the std::regex/boost::regex it replaced, PCRE2 gives a hard
	// backtracking bound (its default match limit), so a pathological
	// user-supplied pattern fails the match instead of hanging the process.
	class TOOLS_DLL Regex
	{
	public:
		struct Options
		{
			bool caseInsensitive = false;
			// Interpret pattern and subject as UTF-8 (code-point semantics,
			// Unicode-aware case folding). Turn OFF to match raw bytes, e.g.
			// against source files whose encoding is unknown.
			bool utf = true;
			// '^' and '$' match at embedded line boundaries.
			bool multiline = false;
			// Anchor both ends so the whole subject must match
			// (std::regex_match semantics rather than regex_search).
			bool fullMatch = false;
		};

		// Throws Tools::ToolsException if the pattern fails to compile.
		explicit Regex(std::string_view pattern, const Options& options = {});
		~Regex();

		Regex(Regex&&) noexcept;
		Regex& operator=(Regex&&) noexcept;

		Regex(const Regex&) = delete;
		Regex& operator=(const Regex&) = delete;

		// True if the pattern matches (anywhere, or the whole subject when
		// Options::fullMatch is set).
		bool Matches(std::string_view subject) const;

		// Returns the capture groups on a match: index 0 is the whole match,
		// 1..N the parenthesised groups. Groups that did not participate are
		// std::nullopt. Returns an empty vector when the pattern does not match.
		std::vector<std::optional<std::string>> Capture(std::string_view subject) const;

	private:
		void Reset() noexcept;

		void* code_ = nullptr; // pcre2_code_8*
	};
}
