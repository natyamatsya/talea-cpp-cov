// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 OpenCppCoverage

#pragma once

#include "ToolsExport.hpp"

namespace Tools
{
	TOOLS_DLL void CreateMiniDumpOnUnHandledException();

	// Suppress the Windows Error Reporting / hard-error dialog boxes (including
	// the classic GP-fault box) so a crashing target process or test fixture
	// cannot block an unattended run. The error mode is inherited by child
	// processes, so every process spawned afterwards is covered as well.
	TOOLS_DLL void DisableCrashDialogs();
}


