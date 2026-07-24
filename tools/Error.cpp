// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2014 OpenCppCoverage
// SPDX-FileCopyrightText: 2026 natyamatsya and talea-cpp-cov contributors

#include "stdafx.h"
#include "Error.hpp"

#include "ToolsException.hpp"
#include "Tool.hpp"

namespace Tools
{
	//-------------------------------------------------------------------------
	void ThrowError(const Error& error)
	{
		throw ToolsException(error.message);
	}

	//-------------------------------------------------------------------------
	Error ErrorFromException(const std::exception& e)
	{
		return Error{Tools::LocalToWString(e.what())};
	}
}
