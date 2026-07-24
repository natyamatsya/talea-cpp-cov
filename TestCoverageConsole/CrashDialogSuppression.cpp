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

#include "Tools/MiniDump.hpp"

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
