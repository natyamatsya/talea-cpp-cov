// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2017 OpenCppCoverage

#pragma once
#include <cstdint>

#include "FileFilterExport.hpp"
#include <filesystem>
#include <vector>
#include <string>

#include "tools/Regex.hpp"

namespace Tools
{
	class MappedFile;
}

namespace FileFilter
{	
	class FileInfo;
	class LineInfo;

	class FILEFILTER_DLL LineFilter
	{
	public:
		explicit LineFilter(
			const std::vector<std::wstring>& excludedLineRegexes,
			bool enableLog = true);
		~LineFilter();

		bool IsLineSelected(const FileInfo&, const LineInfo&);
		bool IsLineSelected(const std::filesystem::path&, std::int32_t lineNumber);
		std::int32_t GetFileReadCount() const;

	private:
		LineFilter(const LineFilter&) = delete;
		LineFilter& operator=(const LineFilter&) = delete;
		LineFilter(LineFilter&&) = delete;
		LineFilter& operator=(LineFilter&&) = delete;

		const std::vector<std::string>* GetLines(const std::filesystem::path&);
		bool IsLineSelected(const std::string& line) const;

		std::vector<Tools::Regex> excludedLineRegexes_;
		std::filesystem::path filePath_;
		std::unique_ptr<Tools::MappedFile> mappedFileForFilePath_;
		std::int32_t fileReadCount_;
		const bool enableLog_;
	};
}
