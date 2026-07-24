// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2016 OpenCppCoverage

#pragma once
#include <cstdint>

#include <vector>

namespace FileFilter
{	
	class LineInfo
	{
	public:
		LineInfo(
			std::int32_t lineNumber,
			DWORD64 virtualAddress,
			ULONG symbolIndex)
			: lineNumber_{ lineNumber }
			, symbolIndex_{ symbolIndex }
			, virtualAddress_{ virtualAddress }
		{}

		const std::int32_t lineNumber_;
		const ULONG symbolIndex_;
		const DWORD64 virtualAddress_;
	};
}