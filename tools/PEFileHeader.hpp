// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2017 OpenCppCoverage

#pragma once

#include "ToolsExport.hpp"
#include "Error.hpp"

#include <Windows.h>

namespace Tools
{
	class IPEFileHeaderHandler
	{
	  public:
		virtual ~IPEFileHeaderHandler() = default;
		virtual void OnNtHeader32(HANDLE hProcess,
		                          DWORD64 baseOfImage,
		                          const IMAGE_NT_HEADERS32&) = 0;
		virtual void OnNtHeader64(HANDLE hProcess,
		                          DWORD64 baseOfImage,
		                          const IMAGE_NT_HEADERS64&) = 0;
	};

	class TOOLS_DLL PEFileHeader
	{
	  public:
		[[nodiscard]] Expected<void>
		Load(HANDLE hProcess, DWORD64 baseOfImage, IPEFileHeaderHandler&) const;
	};
}