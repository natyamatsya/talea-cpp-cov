// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2019 OpenCppCoverage

#include "pch.h"
#include <tchar.h>

//-----------------------------------------------------------------------------
int _tmain(int argc, _TCHAR* argv[])
{
	::testing::InitGoogleMock(&argc, argv);
	return RUN_ALL_TESTS();
}