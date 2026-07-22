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

#include <cstdint>

// Fixture for the compiler-instrumentation ("fast mode") coverage path. It is
// compiled with /fsanitize-coverage=inline-8bit-counters and linked with the
// MSVC fast-mode runtime (see CoverageRuntime), so running it writes a dump to
// OPENCPPCOVERAGE_FASTCOV_OUTPUT that FastCoverageImporter reads back.
//
// The lines are laid out so a run exercises one branch and skips the other,
// giving a deterministic covered / not-covered split the test asserts on. Keep
// the line numbers below stable (GetTakenLine anchors to them).

namespace TestFastCovConsole
{
	// Never inlined, so the two branches keep distinct line records.
	__declspec(noinline) std::int32_t Classify(std::int32_t value)
	{
		if (value > 0)         // taken
			return value + 1;  // taken
		return -1;             // not taken
	}
}

int main(int argc, char*[])
{
	// argc is always >= 1, so the positive branch is taken and the negative
	// one is not - but the optimizer cannot prove it.
	return TestFastCovConsole::Classify(argc) > 0 ? 0 : 1;
}
