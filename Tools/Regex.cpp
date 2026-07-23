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
#include "Regex.hpp"

#include <cstdint>
#include <utility>

#include "Tools/Tool.hpp"
#include "ToolsException.hpp"

// PCRE2 is linked statically (see custom-triplets/x64-windows.cmake), so the
// header must not decorate its API with __declspec(dllimport).
#ifndef PCRE2_STATIC
#define PCRE2_STATIC
#endif
#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>

namespace Tools
{
	namespace
	{
		//-------------------------------------------------------------------------
		PCRE2_SPTR AsBytes(std::string_view str)
		{
			return reinterpret_cast<PCRE2_SPTR>(str.data());
		}

		//-------------------------------------------------------------------------
		// Returns the compiled pcre2_code* (as void*) or a Tools::Error; never
		// throws, so both Regex::Compile and the throwing ctor build on it.
		Expected<void*> CompileCode(std::string_view pattern,
		                            const Regex::Options& options)
		{
			std::uint32_t compileOptions = 0;
			if (options.caseInsensitive)
				compileOptions |= PCRE2_CASELESS;
			if (options.utf)
				compileOptions |= PCRE2_UTF | PCRE2_UCP;
			if (options.multiline)
				compileOptions |= PCRE2_MULTILINE;
			if (options.fullMatch)
				compileOptions |= PCRE2_ANCHORED | PCRE2_ENDANCHORED;

			std::int32_t errorCode = 0;
			PCRE2_SIZE errorOffset = 0;
			auto* code = pcre2_compile(AsBytes(pattern),
			                           pattern.size(),
			                           compileOptions,
			                           &errorCode,
			                           &errorOffset,
			                           nullptr);
			if (!code)
			{
				PCRE2_UCHAR8 buffer[256];
				pcre2_get_error_message(
				    errorCode, buffer, sizeof(buffer) / sizeof(buffer[0]));
				auto message = Tools::Utf8ToWString(
				    reinterpret_cast<const char*>(buffer));
				return std::unexpected(
				    MAKE_ERROR(L"Invalid regular expression at offset "
				               << errorOffset << L": " << message));
			}
			return static_cast<void*>(code);
		}
	}

	//-------------------------------------------------------------------------
	Expected<Regex> Regex::Compile(std::string_view pattern)
	{
		return Compile(pattern, Options{});
	}

	//-------------------------------------------------------------------------
	Expected<Regex> Regex::Compile(std::string_view pattern,
	                               const Options& options)
	{
		auto code = CompileCode(pattern, options);
		if (!code)
			return std::unexpected(std::move(code.error()));
		return Regex{*code};
	}

	//-------------------------------------------------------------------------
	Regex::Regex(void* code) noexcept : code_{code}
	{
	}

	//-------------------------------------------------------------------------
	Regex::Regex(std::string_view pattern) : Regex{pattern, Options{}}
	{
	}

	//-------------------------------------------------------------------------
	Regex::Regex(std::string_view pattern, const Options& options)
		: code_{Tools::ValueOrThrow(CompileCode(pattern, options))}
	{
	}

	//-------------------------------------------------------------------------
	Regex::~Regex()
	{
		Reset();
	}

	//-------------------------------------------------------------------------
	Regex::Regex(Regex&& other) noexcept
		: code_{std::exchange(other.code_, nullptr)}
	{
	}

	//-------------------------------------------------------------------------
	Regex& Regex::operator=(Regex&& other) noexcept
	{
		if (this != &other)
		{
			Reset();
			code_ = std::exchange(other.code_, nullptr);
		}
		return *this;
	}

	//-------------------------------------------------------------------------
	void Regex::Reset() noexcept
	{
		if (code_)
		{
			pcre2_code_free(static_cast<pcre2_code*>(code_));
			code_ = nullptr;
		}
	}

	//-------------------------------------------------------------------------
	bool Regex::Matches(std::string_view subject) const
	{
		auto* code = static_cast<pcre2_code*>(code_);
		auto* matchData = pcre2_match_data_create_from_pattern(code, nullptr);

		// A null subject.data() (empty string_view) is not a valid pointer for
		// PCRE2; hand it a valid empty buffer instead.
		auto* start = subject.empty() ? AsBytes(std::string_view{""})
		                              : AsBytes(subject);
		std::int32_t rc = pcre2_match(
		    code, start, subject.size(), 0, 0, matchData, nullptr);

		pcre2_match_data_free(matchData);
		return rc >= 0;
	}

	//-------------------------------------------------------------------------
	std::vector<std::optional<std::string>>
	Regex::Capture(std::string_view subject) const
	{
		auto* code = static_cast<pcre2_code*>(code_);
		auto* matchData = pcre2_match_data_create_from_pattern(code, nullptr);

		auto* start = subject.empty() ? AsBytes(std::string_view{""})
		                              : AsBytes(subject);
		std::int32_t rc = pcre2_match(
		    code, start, subject.size(), 0, 0, matchData, nullptr);

		std::vector<std::optional<std::string>> groups;
		if (rc >= 0)
		{
			std::uint32_t pairs = pcre2_get_ovector_count(matchData);
			const PCRE2_SIZE* ovector = pcre2_get_ovector_pointer(matchData);
			groups.reserve(pairs);
			for (std::uint32_t i = 0; i < pairs; ++i)
			{
				// Groups numbered >= rc did not participate in the match; PCRE2
				// leaves their ovector slots undefined, so treat them (and any
				// explicitly unset slot) as absent.
				const PCRE2_SIZE begin = ovector[2 * i];
				if (static_cast<std::int32_t>(i) >= rc || begin == PCRE2_UNSET)
					groups.emplace_back(std::nullopt);
				else
					groups.emplace_back(std::string{
					    subject.substr(begin, ovector[2 * i + 1] - begin)});
			}
		}

		pcre2_match_data_free(matchData);
		return groups;
	}
}
