// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2014 OpenCppCoverage
// SPDX-FileCopyrightText: 2026 natyamatsya and talea-cpp-cov contributors

#pragma once

namespace TestCoverageConsole
{
	// Compile-time-constant conditions the optimizer cannot fold. cl emits
	// code and a line record for a literal `if (true)` / `if (false)` at /Od;
	// clang-cl folds the constant, dropping the condition's line record (and,
	// for `if (false)`, the whole branch, so symbol-keeping tricks stop
	// working). Guarding fixtures with these keeps the same lines on both
	// compilers while preserving the taken / not-taken behaviour. Defined out
	// of line (noinline) so each guard site gets a single line record, not a
	// second one from an inlined body.
	__declspec(noinline) inline bool NeverTrue()
	{
		static volatile bool value = false;
		return value;
	}

	__declspec(noinline) inline bool AlwaysTrue()
	{
		static volatile bool value = true;
		return value;
	}
}
