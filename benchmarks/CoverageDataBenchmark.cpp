// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 natyamatsya and talea-cpp-cov contributors

#include <cstdint>
#include <benchmark/benchmark.h>

#include <filesystem>
#include <format>
#include <iterator>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include <fmt/format.h>
#include <fmt/xchar.h>

#include "coverage/CoverageData.hpp"
#include "coverage/ModuleCoverage.hpp"
#include "coverage/FileCoverage.hpp"
#include "coverage/LineCoverage.hpp"
#include "talea-cpp-cov/CoverageDataMerger.hpp"
#include "talea-cpp-cov/RawPdbReader.hpp"
#include "talea-cpp-cov/PdbLineTableCache.hpp"
#include "exporter/binary/CoverageDataSerializer.hpp"
#include "exporter/binary/CoverageDataDeserializer.hpp"
#include "exporter/binary/ColumnarCoverage.hpp"
#include "exporter/CoberturaExporter.hpp"

namespace
{
	const std::int32_t ModuleCount = 3;
	const std::int32_t FileCount = 20;
	const std::int32_t LineCount = 500;

	//-------------------------------------------------------------------------
	Coverage::CoverageData CreateCoverageData()
	{
		Coverage::CoverageData coverageData{L"Benchmark", 0};

		for (std::int32_t moduleIndex = 0; moduleIndex < ModuleCount; ++moduleIndex)
		{
			auto& module =
			    coverageData.AddModule(L"Module" + std::to_wstring(moduleIndex));

			for (std::int32_t fileIndex = 0; fileIndex < FileCount; ++fileIndex)
			{
				auto& file =
				    module.AddFile(L"File" + std::to_wstring(fileIndex));

				for (std::int32_t line = 1; line <= LineCount; ++line)
				{
					file.AddLine(line, line % 2 == 0);
					if (line % 10 == 0)
					{
						file.AddBranch(
						    line,
						    Coverage::BranchCoverage{0, true, line % 20 == 0});
					}
				}
			}
		}
		return coverageData;
	}

	//-------------------------------------------------------------------------
	void MergeCoverageData(benchmark::State& state)
	{
		std::vector<Coverage::CoverageData> coverageDataCollection;
		coverageDataCollection.push_back(CreateCoverageData());
		coverageDataCollection.push_back(CreateCoverageData());

		for (auto _ : state)
		{
			auto merged =
			    TaleaCppCov::CoverageDataMerger{}.Merge(coverageDataCollection);
			benchmark::DoNotOptimize(merged);
		}
	}

	//-------------------------------------------------------------------------
	void SerializeAndDeserializeCoverageData(benchmark::State& state)
	{
		auto coverageData = CreateCoverageData();
		auto path = std::filesystem::temp_directory_path() /
		            "TaleaCppCovAppBenchmark.cov";

		for (auto _ : state)
		{
			Exporter::CoverageDataSerializer{}.Serialize(coverageData, path);
			auto restored =
			    Exporter::CoverageDataDeserializer{}.Deserialize(path, "").value();
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
		            "TaleaCppCovAppColumnarBenchmark.cov";
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
	// --- Cobertura per-line formatting: wostringstream vs std::format vs fmt -
	// The Cobertura exporter's hot inner loop writes a <line/> element per
	// source line. These benchmarks format 100k such lines the pre-modernization
	// way (wostringstream + to_wstring + per-value XML escape) versus a single
	// format_to into a wide buffer. Both {}-formatters are ~4x faster than the
	// stream; fmt is a further ~10% over std::format, which is why the exporter
	// uses fmt. This trio guards against regressing.
	constexpr std::int32_t LineFormatCount = 100000;

	//---------------------------------------------------------------------
	// The exporter escapes every attribute value; replicated so the current
	// baseline pays the same cost it does today.
	std::wstring EscapeXml(const std::wstring& value)
	{
		std::wstring result;
		result.reserve(value.size());
		for (auto c : value)
		{
			switch (c)
			{
				case L'&': result += L"&amp;"; break;
				case L'<': result += L"&lt;"; break;
				case L'>': result += L"&gt;"; break;
				case L'\'': result += L"&apos;"; break;
				case L'"': result += L"&quot;"; break;
				default: result += c; break;
			}
		}
		return result;
	}

	//---------------------------------------------------------------------
	std::vector<std::pair<std::int32_t, bool>> MakeLines()
	{
		std::vector<std::pair<std::int32_t, bool>> lines;
		lines.reserve(LineFormatCount);
		for (std::int32_t i = 1; i <= LineFormatCount; ++i)
			lines.emplace_back(i, i % 2 == 0);
		return lines;
	}

	//---------------------------------------------------------------------
	// Named "Legacy" because it is the pre-modernization baseline kept for
	// comparison, NOT what the exporter ships: production formats the hot
	// integer attributes with fmt::format_to (see CoberturaExporter.cpp),
	// closer to the *_Fmt numbers below.
	void CoberturaLine_Legacy(benchmark::State& state)
	{
		auto lines = MakeLines();
		for (auto _ : state)
		{
			std::wostringstream ostr;
			for (const auto& [number, executed] : lines)
			{
				for (std::int32_t i = 0; i < 8; ++i)
					ostr << L' ';
				ostr << L'<' << L"line";
				ostr << L' ' << L"number" << L"=\""
				     << EscapeXml(std::to_wstring(number)) << L"\"";
				ostr << L' ' << L"hits" << L"=\""
				     << EscapeXml(executed ? L"1" : L"0") << L"\"";
				ostr << L"/>\n";
			}
			auto result = ostr.str();
			benchmark::DoNotOptimize(result);
		}
	}

	//---------------------------------------------------------------------
	void CoberturaLine_StdFormat(benchmark::State& state)
	{
		auto lines = MakeLines();
		for (auto _ : state)
		{
			std::wstring buffer;
			for (const auto& [number, executed] : lines)
				std::format_to(
				    std::back_inserter(buffer),
				    L"        <line number=\"{}\" hits=\"{}\"/>\n",
				    number,
				    executed ? 1 : 0);
			benchmark::DoNotOptimize(buffer);
		}
	}

	//---------------------------------------------------------------------
	void CoberturaLine_Fmt(benchmark::State& state)
	{
		auto lines = MakeLines();
		for (auto _ : state)
		{
			std::wstring buffer;
			for (const auto& [number, executed] : lines)
				fmt::format_to(
				    std::back_inserter(buffer),
				    L"        <line number=\"{}\" hits=\"{}\"/>\n",
				    number,
				    executed ? 1 : 0);
			benchmark::DoNotOptimize(buffer);
		}
	}

	//-------------------------------------------------------------------------
	// Cold PDB line-table parse: the dominant per-module startup cost, and the
	// raw_pdb reader is the default collection path (--no_raw_pdb falls back to
	// DIA). Parses TaleaCppCov.dll's own PDB - a large, representative module -
	// fresh each iteration. The OS keeps the file warm, so this measures parse
	// CPU, not disk I/O.
	void ParsePdbRawPdb(benchmark::State& state)
	{
		const auto module = std::filesystem::path{OUT_DIR} / L"TaleaCppCov.dll";
		if (!std::filesystem::exists(module))
		{
			state.SkipWithError("TaleaCppCov.dll not found next to the benchmark");
			return;
		}

		std::size_t fileCount = 0;
		for (auto _ : state)
		{
			TaleaCppCov::ModuleLineTable table;
			if (!TaleaCppCov::RawPdbReader::TryRead(module, table))
			{
				state.SkipWithError("RawPdbReader::TryRead failed");
				return;
			}
			fileCount = table.files_.size();
			benchmark::DoNotOptimize(table);
		}
		state.counters["source_files"] =
		    benchmark::Counter(static_cast<double>(fileCount));
	}

	//-------------------------------------------------------------------------
	void ExportCobertura(benchmark::State& state)
	{
		auto coverageData = CreateCoverageData();

		for (auto _ : state)
		{
			std::ostringstream ostr;
			Exporter::CoberturaExporter{}.Export(coverageData, ostr);
			benchmark::DoNotOptimize(ostr);
		}
	}
}

BENCHMARK(MergeCoverageData);
BENCHMARK(MergeColumnarShards)->Arg(2)->Arg(10)->Arg(30);
BENCHMARK(SerializeAndDeserializeCoverageData);
BENCHMARK(ExportCobertura);
BENCHMARK(CoberturaLine_Legacy);
BENCHMARK(CoberturaLine_StdFormat);
BENCHMARK(CoberturaLine_Fmt);
BENCHMARK(ParsePdbRawPdb);
