// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 natyamatsya and talea-cpp-cov contributors

#pragma once
#include <cstdint>

#include <filesystem>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "TaleaCppCovExport.hpp"
#include "DebugInformationEnumerator.hpp"
#include "PdbSignature.hpp"

namespace TaleaCppCov
{
	// Every (source file, line record) of one module as enumerated from its
	// PDB, before any source-file selection or path substitution — so one
	// table stays valid whatever filters a run uses. Immutable once built,
	// shared across module loads.
	struct ModuleLineTable
	{
		struct File
		{
			// The file name exactly as stored in the PDB; substitute paths
			// are applied when the table is replayed, not when it is built.
			std::wstring rawName_;
			std::vector<IDebugInformationHandler::Line> lines_;
		};

		std::vector<File> files_; // in PDB source-file enumeration order
		// Function code ranges, sorted by address.
		std::vector<IDebugInformationHandler::FunctionRange> functions_;
		bool hasSymbolIndexes_ = false;
	};

	// Read-only view of a module's line table. Two backings share one
	// interface (MEMORY_ROADMAP.md, Tier 1):
	//   - a parser-built, heap-resident ModuleLineTable;
	//   - a disk-cache entry consumed IN PLACE through a file mapping — the
	//     line records (the bulk of a table) then live in clean file-backed
	//     pages: no commit charge, evicted and re-read transparently under
	//     memory pressure, shared between concurrent runs.
	class TALEACPPCOV_DLL LineTableView
	{
	  public:
		using Line = IDebugInformationHandler::Line;
		using FunctionRange = IDebugInformationHandler::FunctionRange;

		// Wraps a parser-built table.
		explicit LineTableView(std::shared_ptr<const ModuleLineTable>);

		bool HasSymbolIndexes() const;
		const std::vector<FunctionRange>& Functions() const;
		std::size_t FileCount() const;
		const std::wstring& FileRawName(std::size_t fileIndex) const;

		// One file's line records. The heap backing returns its vector
		// directly; the mapped backing decodes the file's slice of the
		// mapped blob into `scratch` (one small, transient copy per
		// replayed file — the whole table never becomes heap-resident).
		const std::vector<Line>& FileLines(std::size_t fileIndex,
		                                   std::vector<Line>& scratch) const;

	  private:
		friend class PdbLineTableCache;
		LineTableView() = default;

		struct MappedFile
		{
			std::wstring rawName_;
			std::uint64_t firstLine_ = 0;
			std::uint32_t lineCount_ = 0;
		};

		// Heap backing (set) — or mapped backing (null; the members below).
		std::shared_ptr<const ModuleLineTable> table_;
		std::shared_ptr<const void> mapping_; // keeps mappedLines_ alive
		const Line* mappedLines_ = nullptr;   // into mapping_, 8-aligned
		std::vector<MappedFile> mappedFiles_;
		std::vector<FunctionRange> mappedFunctions_;
		bool mappedHasSymbolIndexes_ = false;
	};

	// Flyweight for parsed PDB line tables, keyed by the PDB signature
	// (GUID + age): a module's debug information is parsed once and replayed
	// for every later load — the same DLL in a child process, a repeated
	// load/unload, or, through the disk layer, a later run.
	class TALEACPPCOV_DLL PdbLineTableCache
	{
	  public:
		// An empty directory disables the disk layer (in-memory only).
		explicit PdbLineTableCache(
		    std::filesystem::path diskCacheDirectory = {});

		// %LOCALAPPDATA%\TaleaCppCovApp\PdbCache
		static std::filesystem::path GetDefaultDiskCacheDirectory();

		// A cached table qualifies when it carries symbol indexes or the
		// caller does not need them. Thread-safe: the import-closure pre-parse
		// warms the cache from several threads at once, concurrently with the
		// module loads that read it.
		std::shared_ptr<const LineTableView> TryGet(const PdbSignature&,
		                                            bool needSymbolIndexes);

		// Saves to the disk layer and caches a view for later TryGets —
		// preferring a mapping of the just-written cache file over the heap
		// table, so even the run that parsed a module keeps only file-backed
		// pages once the module is stored.
		void Store(const PdbSignature&,
		           std::shared_ptr<const ModuleLineTable>);

		std::int32_t GetMemoryHitCount() const;
		std::int32_t GetDiskHitCount() const;
		std::int32_t GetMissCount() const;

	  private:
		PdbLineTableCache(const PdbLineTableCache&) = delete;
		PdbLineTableCache& operator=(const PdbLineTableCache&) = delete;

		std::filesystem::path GetCacheFilePath(const PdbSignature&,
		                                       bool withSymbolIndexes) const;
		std::shared_ptr<const LineTableView>
		TryLoadFromDisk(const std::filesystem::path&) const;
		bool SaveToDisk(const PdbSignature&, const ModuleLineTable&) const;
		void PruneDiskCache() const;

		const std::filesystem::path diskCacheDirectory_;
		mutable std::mutex mutex_; // guards views_ and the hit counters
		std::map<std::wstring, std::shared_ptr<const LineTableView>> views_;
		std::int32_t memoryHitCount_ = 0;
		std::int32_t diskHitCount_ = 0;
		std::int32_t missCount_ = 0;
	};
}
