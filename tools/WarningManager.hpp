// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2018 OpenCppCoverage

#pragma once

#include "ToolsExport.hpp"
#include <string>
#include <vector>

namespace Tools
{
	class TOOLS_DLL WarningManager
	{
	public:
		WarningManager() = default;

		void AddWarning(const std::wstring&);
		void DisplayWarnings() const;
		
	private:
		std::vector<std::wstring> warnings_;
	};
}