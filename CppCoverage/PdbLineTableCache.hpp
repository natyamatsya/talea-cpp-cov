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

#include <filesystem>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "CppCoverageExport.hpp"
#include "DebugInformationEnumerator.hpp"
#include "PdbSignature.hpp"

namespace CppCoverage
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
		bool hasSymbolIndexes_ = false;
	};

	// Flyweight for parsed PDB line tables, keyed by the PDB signature
	// (GUID + age): a module's debug information is parsed once and replayed
	// for every later load — the same DLL in a child process, a repeated
	// load/unload, or, through the disk layer, a later run.
	class CPPCOVERAGE_DLL PdbLineTableCache
	{
	  public:
		// An empty directory disables the disk layer (in-memory only).
		explicit PdbLineTableCache(
		    std::filesystem::path diskCacheDirectory = {});

		// %LOCALAPPDATA%\OpenCppCoverage\PdbCache
		static std::filesystem::path GetDefaultDiskCacheDirectory();

		// A cached table qualifies when it carries symbol indexes or the
		// caller does not need them.
		std::shared_ptr<const ModuleLineTable> TryGet(const PdbSignature&,
		                                              bool needSymbolIndexes);

		void Store(const PdbSignature&,
		           std::shared_ptr<const ModuleLineTable>);

		int GetMemoryHitCount() const;
		int GetDiskHitCount() const;
		int GetMissCount() const;

	  private:
		PdbLineTableCache(const PdbLineTableCache&) = delete;
		PdbLineTableCache& operator=(const PdbLineTableCache&) = delete;

		std::filesystem::path GetCacheFilePath(const PdbSignature&,
		                                       bool withSymbolIndexes) const;
		std::shared_ptr<const ModuleLineTable>
		TryLoadFromDisk(const std::filesystem::path&) const;
		void SaveToDisk(const PdbSignature&, const ModuleLineTable&) const;
		void PruneDiskCache() const;

		const std::filesystem::path diskCacheDirectory_;
		std::map<std::wstring, std::shared_ptr<const ModuleLineTable>>
		    tables_;
		int memoryHitCount_ = 0;
		int diskHitCount_ = 0;
		int missCount_ = 0;
	};
}
