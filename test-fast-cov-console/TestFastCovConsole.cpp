// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 natyamatsya and talea-cpp-cov contributors

#include <cstdint>

// Fixture for the compiler-instrumentation ("fast mode") coverage path. It is
// compiled with /fsanitize-coverage=inline-8bit-counters and linked with the
// MSVC fast-mode runtime (see CoverageRuntime), so running it writes a dump to
// TALEA_CPP_COV_FASTCOV_OUTPUT that FastCoverageImporter reads back.
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
