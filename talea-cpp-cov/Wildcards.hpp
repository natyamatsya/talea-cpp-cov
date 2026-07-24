// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2014 OpenCppCoverage

#pragma once

#include <string>

#include "tools/Regex.hpp"

#include "TaleaCppCovExport.hpp"

namespace TaleaCppCov
{
	class TALEACPPCOV_DLL Wildcards
	{
	public:
		static const std::vector<wchar_t> EscapedChars;

	public:
		explicit Wildcards(std::wstring, bool isRegexCaseSensitiv = false);
		Wildcards(Wildcards&&);

		bool Match(const std::wstring& str) const;

		friend TALEACPPCOV_DLL std::wostream& operator<<(std::wostream&, const Wildcards&);

	private:
		Wildcards(const Wildcards&) = delete;
		Wildcards& operator=(const Wildcards&) = delete;

	private:
		Tools::Regex wildcards_;
		std::wstring originalStr_;
	};
}