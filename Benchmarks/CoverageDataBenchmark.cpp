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

#include <benchmark/benchmark.h>

#include <filesystem>
#include <sstream>
#include <vector>

#include "Plugin/Exporter/CoverageData.hpp"
#include "Plugin/Exporter/ModuleCoverage.hpp"
#include "Plugin/Exporter/FileCoverage.hpp"
#include "Plugin/Exporter/LineCoverage.hpp"
#include "CppCoverage/CoverageDataMerger.hpp"
#include "Exporter/Binary/CoverageDataSerializer.hpp"
#include "Exporter/Binary/CoverageDataDeserializer.hpp"
#include "Exporter/Binary/ColumnarCoverage.hpp"
#include "Exporter/CoberturaExporter.hpp"

namespace
{
	const int ModuleCount = 3;
	const int FileCount = 20;
	const int LineCount = 500;

	//-------------------------------------------------------------------------
	Plugin::CoverageData CreateCoverageData()
	{
		Plugin::CoverageData coverageData{L"Benchmark", 0};

		for (int moduleIndex = 0; moduleIndex < ModuleCount; ++moduleIndex)
		{
			auto& module =
			    coverageData.AddModule(L"Module" + std::to_wstring(moduleIndex));

			for (int fileIndex = 0; fileIndex < FileCount; ++fileIndex)
			{
				auto& file =
				    module.AddFile(L"File" + std::to_wstring(fileIndex));

				for (int line = 1; line <= LineCount; ++line)
				{
					file.AddLine(line, line % 2 == 0);
					if (line % 10 == 0)
					{
						file.AddBranch(
						    line,
						    Plugin::BranchCoverage{0, true, line % 20 == 0});
					}
				}
			}
		}
		return coverageData;
	}

	//-------------------------------------------------------------------------
	void MergeCoverageData(benchmark::State& state)
	{
		std::vector<Plugin::CoverageData> coverageDataCollection;
		coverageDataCollection.push_back(CreateCoverageData());
		coverageDataCollection.push_back(CreateCoverageData());

		for (auto _ : state)
		{
			auto merged =
			    CppCoverage::CoverageDataMerger{}.Merge(coverageDataCollection);
			benchmark::DoNotOptimize(merged);
		}
	}

	//-------------------------------------------------------------------------
	void SerializeAndDeserializeCoverageData(benchmark::State& state)
	{
		auto coverageData = CreateCoverageData();
		auto path = std::filesystem::temp_directory_path() /
		            "OpenCppCoverageBenchmark.cov";

		for (auto _ : state)
		{
			Exporter::CoverageDataSerializer{}.Serialize(coverageData, path);
			auto restored =
			    Exporter::CoverageDataDeserializer{}.Deserialize(path, "");
			benchmark::DoNotOptimize(restored);
		}
		std::filesystem::remove(path);
	}

	//-------------------------------------------------------------------------
	// The columnar `.cov` v2 shard merge that ships for --input_coverage: read
	// N same-build shards straight into the accumulator and OR their bitmaps.
	// The shard count is the benchmark argument.
	void MergeColumnarShards(benchmark::State& state)
	{
		auto coverageData = CreateCoverageData();
		auto path = std::filesystem::temp_directory_path() /
		            "OpenCppCoverageColumnarBenchmark.cov";
		// The serializer writes the columnar v2 format.
		Exporter::CoverageDataSerializer{}.Serialize(coverageData, path);

		const auto shardCount = state.range(0);
		for (auto _ : state)
		{
			Exporter::ColumnarCoverageMerger merger;
			for (std::int64_t i = 0; i < shardCount; ++i)
				merger.AddColumnarFile(path);
			auto merged = merger.ToCoverageData();
			benchmark::DoNotOptimize(merged);
		}
		std::filesystem::remove(path);
	}

	//-------------------------------------------------------------------------
	void ExportCobertura(benchmark::State& state)
	{
		auto coverageData = CreateCoverageData();

		for (auto _ : state)
		{
			std::wostringstream ostr;
			Exporter::CoberturaExporter{}.Export(coverageData, ostr);
			benchmark::DoNotOptimize(ostr);
		}
	}
}

BENCHMARK(MergeCoverageData);
BENCHMARK(MergeColumnarShards)->Arg(2)->Arg(10)->Arg(30);
BENCHMARK(SerializeAndDeserializeCoverageData);
BENCHMARK(ExportCobertura);
