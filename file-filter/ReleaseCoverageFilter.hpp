// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2016 OpenCppCoverage

#pragma once
#include <cstdint>

#include "FileFilterExport.hpp"
#include <unordered_set>
#include <unordered_map>
#include <memory>
#include <filesystem>

namespace FileFilter
{	
	class IRelocationsExtractor;
	class ModuleInfo;
	class FileInfo;
	class LineInfo;

	class FILEFILTER_DLL ReleaseCoverageFilter
	{
	public:
		ReleaseCoverageFilter();
		explicit ReleaseCoverageFilter(std::unique_ptr<IRelocationsExtractor>);
		~ReleaseCoverageFilter();

		bool IsLineSelected(const ModuleInfo&, const FileInfo&, const LineInfo&);
		
	private:
		ReleaseCoverageFilter(const ReleaseCoverageFilter&) = delete;
		ReleaseCoverageFilter& operator=(const ReleaseCoverageFilter&) = delete;
		ReleaseCoverageFilter(ReleaseCoverageFilter&&) = delete;
		ReleaseCoverageFilter& operator=(ReleaseCoverageFilter&&) = delete;

		void UpdateCachesIfExpired(const ModuleInfo&, const FileInfo&);
		struct FileData;
		std::unique_ptr<FileData>
		UpdateLineDataCaches(const std::filesystem::path& filePath,
		                     const std::vector<LineInfo>&);

		const std::unique_ptr<IRelocationsExtractor> relocationsExtractor_;

		struct FileData
		{
			std::filesystem::path path_;
			std::unordered_set<DWORD64> lastSymbolAddresses_;
			std::unordered_map<std::int32_t, std::int32_t> addressCountByLine_;
		};

		struct ModuleData
		{
			std::filesystem::path path_;
			std::unordered_set<DWORD64> relocations_;
			std::unique_ptr<FileData> fileData_;
		};

		std::unique_ptr<ModuleData> mModuleData_;
	};
}

