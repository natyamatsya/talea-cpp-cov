// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2016 OpenCppCoverage

#pragma once

#include <windows.h>
#include <filesystem>

namespace FileFilter
{
	class ModuleInfo
	{
	public:
		//---------------------------------------------------------------------------
	  ModuleInfo(HANDLE hProcess,
		         const std::filesystem::path& path,
		         void* baseOfImage)
		  : hProcess_{hProcess}, path_{path}, baseOfImage_{baseOfImage}
	  {
	  }

	  const HANDLE hProcess_;
	  const std::filesystem::path path_;
	  void* const baseOfImage_;
	};
}
