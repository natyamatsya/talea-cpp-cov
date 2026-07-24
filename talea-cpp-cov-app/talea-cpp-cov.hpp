// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2014 OpenCppCoverage

#pragma once
#include <cstdint>

#include <iosfwd>

namespace TaleaCppCovApp
{
	const std::int32_t FailureExitCode = 0x9F8C8E5C;

	class TaleaCppCovApp
	{
	public:
		TaleaCppCovApp() = default;

		int Run(int argc, const char** argv, std::wostream* emptyOptionsExplanation) const;

	private:
		TaleaCppCovApp(const TaleaCppCovApp&) = delete;
		TaleaCppCovApp& operator=(const TaleaCppCovApp&) = delete;
	};
}

