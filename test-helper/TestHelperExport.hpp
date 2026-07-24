// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2014 OpenCppCoverage

#pragma once

// TestHelper is a static utility library linked into the test executables;
// there is no DLL boundary, so the macro expands to nothing. (The runtime-
// loaded TestHelper.dll is a separate one-function target, TestUnloadHelper.)
#define TEST_HELPER_DLL
