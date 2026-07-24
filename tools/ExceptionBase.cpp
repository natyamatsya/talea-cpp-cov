// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2014 OpenCppCoverage

#include "stdafx.h"
#include "ExceptionBase.hpp"
#include "Tool.hpp"
#include <filesystem>

namespace Tools
{
	//-------------------------------------------------------------------------
	ExceptionBase::ExceptionBase(const std::wstring& message)
		: std::exception(ToLocalString(message).c_str())
	{
	}

	//-------------------------------------------------------------------------
	std::wstring GetFilename(const char* path)
	{
		return std::filesystem::path{ path }.filename().wstring();
	}
}