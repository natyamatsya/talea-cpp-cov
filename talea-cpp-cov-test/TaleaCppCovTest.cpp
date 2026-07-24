// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2014 OpenCppCoverage

#include "stdafx.h"
#include <tchar.h>

#include "tools/Log.hpp"
#include "tools/MiniDump.hpp"

//-----------------------------------------------------------------------------
int _tmain(int argc, _TCHAR* argv[])
{
	// This suite drives the coverage engines over fixtures that deliberately
	// crash (unhandled C++ exceptions, DebugBreak). Suppress the WER dialogs so
	// the spawned children, which inherit this error mode, cannot block the run.
	Tools::DisableCrashDialogs();
	Tools::EnableLogger(false);
	::testing::InitGoogleMock(&argc, argv);
	return RUN_ALL_TESTS();
}

