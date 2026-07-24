// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2014 OpenCppCoverage

#pragma once

#include <Windows.h>

#include "TaleaCppCovExport.hpp"
#include "tools/Error.hpp"

namespace TaleaCppCov
{
	class TALEACPPCOV_DLL HandleInformation
	{
	public:
		HandleInformation();
		[[nodiscard]] Tools::Expected<std::wstring> ComputeFilename(HANDLE hfile) const;

	private:
		HandleInformation(const HandleInformation&) = delete;
		HandleInformation& operator=(const HandleInformation&) = delete;
	};
}


