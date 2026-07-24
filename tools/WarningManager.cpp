// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2018 OpenCppCoverage

#include "stdafx.h"
#include "WarningManager.hpp"
#include "tools/Log.hpp"

namespace Tools
{
	//-------------------------------------------------------------------------
	void WarningManager::AddWarning(const std::wstring& warning)
	{
		warnings_.push_back(warning);
	}

	//-------------------------------------------------------------------------
	void WarningManager::DisplayWarnings() const
	{
		for (const auto& warning : warnings_)
			LOG_WARNING("{}", warning);
	}
}
