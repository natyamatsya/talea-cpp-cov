// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2014 OpenCppCoverage

#pragma once

#include <Windows.h>
#include <optional>

#include "TaleaCppCovExport.hpp"
#include "StartInfo.hpp"

#include "tools/Error.hpp"

namespace TaleaCppCov
{
	class TALEACPPCOV_DLL Process
	{
	public:
		static const std::wstring CannotFindPathMessage;
		static const std::wstring CheckIfValidExecutableMessage;

		Process(const StartInfo& startInfo);
		~Process();

		[[nodiscard]] Tools::Expected<void> Start(DWORD creationFlags);

	private:
		Process(const Process&) = delete;
		Process& operator=(const Process&) = delete;

	private:
		std::optional<PROCESS_INFORMATION> processInformation_;
		const StartInfo startInfo_;
	};
}

