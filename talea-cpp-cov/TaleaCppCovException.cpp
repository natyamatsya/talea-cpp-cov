// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2014 OpenCppCoverage

#include "stdafx.h"
#include <cstdint>
#include "TaleaCppCovException.hpp"

#include <sstream>

#include "tools/Tool.hpp"

namespace TaleaCppCov
{	
	//-------------------------------------------------------------------------
	std::wstring GetErrorMessage(std::int32_t lastErrorCode)
	{			
		std::vector<wchar_t> sysMsg(64 * 1024);
		std::wostringstream ostr;
			
		if (FormatMessage(
				FORMAT_MESSAGE_FROM_SYSTEM,
				NULL, lastErrorCode,
				MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL),
				&sysMsg[0], static_cast<std::int32_t>(sysMsg.size()), NULL))
		{
			ostr << &sysMsg[0];
		}
		else
		{
			ostr << "Last error code:" << lastErrorCode;
		}

		return ostr.str();
	}
}
