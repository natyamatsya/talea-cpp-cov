// OpenCppCoverage is an open source code coverage for C++.
// Copyright (C) 2017 OpenCppCoverage

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

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
