// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 natyamatsya and talea-cpp-cov contributors

#include <cstdint>
#include <benchmark/benchmark.h>

#include <vector>

#include "talea-cpp-cov/BranchDisassembler.hpp"

namespace
{
	//-------------------------------------------------------------------------
	std::vector<std::uint8_t> CreateCodeBuffer(size_t patternCount)
	{
		// cmp eax, ebx; je +2; mov eax, 1; jne -11
		const std::uint8_t pattern[] = {
		    0x3B, 0xC3,                    // cmp eax, ebx
		    0x74, 0x02,                    // je +2
		    0xB8, 0x01, 0x00, 0x00, 0x00,  // mov eax, 1
		    0x75, 0xF5                     // jne -11
		};

		std::vector<std::uint8_t> buffer;
		buffer.reserve(patternCount * sizeof(pattern));
		for (size_t i = 0; i < patternCount; ++i)
			buffer.insert(buffer.end(), std::begin(pattern), std::end(pattern));
		return buffer;
	}

	//-------------------------------------------------------------------------
	void FindConditionalBranches(benchmark::State& state)
	{
		TaleaCppCov::BranchDisassembler disassembler{true};
		auto buffer = CreateCodeBuffer(static_cast<size_t>(state.range(0)));

		for (auto _ : state)
		{
			auto branches = disassembler.FindConditionalBranches(
			    buffer.data(), buffer.size(), 0x1000);
			benchmark::DoNotOptimize(branches);
		}
		state.SetBytesProcessed(state.iterations() * buffer.size());
	}
}

BENCHMARK(FindConditionalBranches)->Range(1, 100000);
