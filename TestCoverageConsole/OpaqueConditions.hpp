// OpenCppCoverage is an open source code coverage for C++.
// Copyright (C) 2026 OpenCppCoverage
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
