// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2014 OpenCppCoverage

#include "stdafx.h"
#include <tchar.h>

#include "tools/Log.hpp"

//-----------------------------------------------------------------------------
int _tmain(int argc, _TCHAR* argv[])
{
	Tools::EnableLogger(false);
	::testing::InitGoogleMock(&argc, argv);
	return RUN_ALL_TESTS();
}