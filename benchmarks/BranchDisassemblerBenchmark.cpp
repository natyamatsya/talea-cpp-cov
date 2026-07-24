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
