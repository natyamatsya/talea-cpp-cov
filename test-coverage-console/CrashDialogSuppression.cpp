// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 natyamatsya and talea-cpp-cov contributors

#include "tools/MiniDump.hpp"

namespace
{
	// This fixture deliberately crashes (uncaught exceptions, DebugBreak) so the
	// coverage engines can be exercised against a crashing target. A blocking
	// crash dialog would hang an unattended test run, so suppress every crash UI
	// before main runs. This is done from a global constructor rather than inside
	// _tmain because CodeCoverageRunnerTest asserts exact line numbers in
	// TestCoverageConsole.cpp; this translation unit adds none.
	const struct CrashDialogSuppressor
	{
		CrashDialogSuppressor() { Tools::DisableCrashDialogs(); }
	} crashDialogSuppressor;
}
