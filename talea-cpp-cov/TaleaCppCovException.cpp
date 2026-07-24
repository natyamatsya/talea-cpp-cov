// OpenCppCoverage is an open source code coverage for C++.
// Copyright (C) 2014 OpenCppCoverage
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

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
