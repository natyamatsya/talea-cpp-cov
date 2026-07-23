// OpenCppCoverage is an open source code coverage for C++.
// Copyright (C) 2014 OpenCppCoverage
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

#include <cstdint>
#include <filesystem>
#include <map>
#include <ostream>
#include <string>
#include <vector>

#include "../ExporterExport.hpp"
#include "Tools/Error.hpp"

namespace Plugin
{
	class CoverageData;
}

namespace Exporter
{
	// Columnar ".cov v2" binary format.
	//
	// The legacy format is a protobuf hierarchy (CoverageData -> per-module
	// messages -> per-file -> per-line objects). Deserializing it rebuilds a
	// map-of-maps for every shard, and merging N shards walks every line of
	// every shard through per-line map lookups. Both cost O(total lines).
	//
	// The columnar layout stores, per (module, file): the file's line numbers
	// as delta-varints and a packed "executed" bitmap (1 bit/line on disk),
	// plus a sparse branch side-table. Shards of the same build share an
	// identical line array, so merging two files is a linear byte-wise OR of
	// their executed arrays (expanded to one byte per line in memory;
	// ColumnarCoverageMerger's fast path) instead of a per-line map merge.
	//
	// Layout (little-endian; varint = unsigned LEB128):
	//   u32 magic ('O''C''C''2'), u32 version
	//   str name (u32 byteLen + utf8), i32 exitCode
	//   u32 stringCount, [str]*        -- interned module + file paths (utf8)
	//   u32 moduleCount
	//     u32 pathId, u32 fileCount
	//       u32 pathId, u32 lineCount
	//       varint[lineCount] deltaLineNumbers   -- ascending, so deltas >= 1
	//       u8[ceil(lineCount/8)] executedBitmap  -- bit i <-> line i
	//       u32 branchLineCount
	//         varint lineNumber, u32 branchCount
	//           varint branchIndex, u8 flags (bit0 taken, bit1 fallthrough)
	namespace ColumnarBinary
	{
		// Raw column data for one file, shared by the reader and the merger.
		struct FileData
		{
			std::vector<std::uint32_t> lines;      // ascending line numbers
			std::vector<std::uint8_t> executed;   // parallel to lines, 0/1
			// lineNumber -> (branchIndex -> flags: bit0 taken, bit1 fallthrough)
			std::map<std::uint32_t, std::map<std::uint32_t, std::uint8_t>> branches;
		};

		constexpr std::uint32_t Magic = 0x3243434F; // "OCC2" little-endian
		constexpr std::uint32_t Version = 1;

		// Writes coverageData in the columnar v2 format, preserving the module
		// and file order of the source.
		EXPORTER_DLL void Write(const Plugin::CoverageData&, std::ostream&);

		// Reads a columnar v2 file back into a CoverageData, preserving the
		// stored module/file order (single-file round-trip; the merger sorts).
		[[nodiscard]] EXPORTER_DLL Tools::Expected<Plugin::CoverageData>
		Read(const std::filesystem::path&);

		// True if the file at path starts with the columnar magic. Cheap: reads
		// only the first four bytes.
		EXPORTER_DLL bool IsColumnarFile(const std::filesystem::path&);

		// Reads a whole columnar file into memory.
		[[nodiscard]] EXPORTER_DLL Tools::Expected<std::vector<char>>
		ReadFile(const std::filesystem::path&);
	}

	// Accumulates coverage from many shards into a single merged CoverageData,
	// OR-ing executed bitmaps rather than rebuilding map-of-maps per shard.
	class EXPORTER_DLL ColumnarCoverageMerger
	{
	public:
		ColumnarCoverageMerger() = default;

		// Merges an already-deserialized CoverageData (used for the live run,
		// fastcov imports, and the legacy v1 fallback).
		void Add(const Plugin::CoverageData&);

		// Reads a columnar v2 file directly into the accumulator, skipping the
		// intermediate CoverageData. Returns false (adding nothing) if the file
		// is not columnar, so the caller can fall back to the v1 deserializer.
		bool AddColumnarFile(const std::filesystem::path&);

		// Produces the merged CoverageData. Modules, files and lines come out
		// sorted by path/line, matching the legacy CoverageDataMerger.
		Plugin::CoverageData ToCoverageData() const;

	private:
		ColumnarCoverageMerger(const ColumnarCoverageMerger&) = delete;
		ColumnarCoverageMerger& operator=(const ColumnarCoverageMerger&) = delete;

		void MergeFile(
			const std::filesystem::path& modulePath,
			const std::filesystem::path& filePath,
			ColumnarBinary::FileData&& incoming);

		// modulePath -> filePath -> merged columns. Ordered maps give the
		// deterministic, sorted output the legacy merger produced.
		std::map<std::filesystem::path,
		         std::map<std::filesystem::path, ColumnarBinary::FileData>>
		    modules_;
		std::wstring name_;
		std::int32_t lastNotZeroExitCode_ = 0;
	};
}
