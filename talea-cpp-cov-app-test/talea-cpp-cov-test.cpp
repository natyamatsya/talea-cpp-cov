// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2014 OpenCppCoverage

#include "stdafx.h"
#include <tchar.h>

#include "tools/MiniDump.hpp"

//-----------------------------------------------------------------------------
int _tmain(int argc, _TCHAR* argv[])
{
	// Runs the real talea-cpp-cov.exe over fixtures that deliberately crash.
	// Suppress the WER dialogs so nothing blocks an unattended run.
	Tools::DisableCrashDialogs();
	::testing::InitGoogleMock(&argc, argv);
	return RUN_ALL_TESTS();
}
