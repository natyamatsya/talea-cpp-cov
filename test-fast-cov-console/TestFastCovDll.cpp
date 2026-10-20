// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 natyamatsya and talea-cpp-cov contributors

#include <cstdint>

// Instrumented DLL fixture for the fast-mode runtime's per-module dumps. The
// host (TestFastCovDllHost) loads it, calls TestFastCovDllClassify once and,
// depending on its argument, unloads it before exit or keeps it loaded. Keep
// the branch layout: the test compares a deterministic covered / not-covered
// split against the debugger.

extern "C" __declspec(dllexport) std::int32_t TestFastCovDllClassify(
    std::int32_t value)
{
	if (value > 0)         // taken
		return value * 2;  // taken
	return -1;             // not taken
}
