// OpenCppCoverage is an open source code coverage for C++.
// Copyright (C) 2017 OpenCppCoverage
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

#include "stdafx.h"
#include "LineFilter.hpp"

#include "FileFilter/FileInfo.hpp"
#include "FileFilter/LineInfo.hpp"
#include "Tools/Log.hpp"
#include "Tools/Tool.hpp"
#include "Tools/MappedFile.hpp"

namespace FileFilter
{
	//-------------------------------------------------------------------------
	LineFilter::LineFilter(
		const std::vector<std::wstring>& excludedLineRegexes,
		bool enableLog)
		: fileReadCount_{0}
		, enableLog_{ enableLog }
	{
		// The source lines are matched as raw bytes (their encoding is
		// unknown), so keep the pattern in the same local encoding and match in
		// byte mode. fullMatch reproduces the previous std::regex_match, i.e. the
		// whole line must match the exclusion pattern.
		for (const auto& regex : excludedLineRegexes)
			excludedLineRegexes_.emplace_back(
			    Tools::ToLocalString(regex),
			    Tools::Regex::Options{.utf = false, .fullMatch = true});
	}

	//-------------------------------------------------------------------------
	bool LineFilter::IsLineSelected(
		const FileInfo& fileInfo,
		const LineInfo& lineInfo)
	{
		return IsLineSelected(fileInfo.filePath_, lineInfo.lineNumber_);
	}

	//-------------------------------------------------------------------------
	LineFilter::~LineFilter() = default;

	//-------------------------------------------------------------------------
	bool LineFilter::IsLineSelected(
		const std::filesystem::path& filePath,
		int lineNumber)
	{
		// Without any exclusion regex there is nothing to check, so avoid
		// reading and line-splitting the whole source file per line record.
		if (excludedLineRegexes_.empty())
			return true;

		const auto* lines = GetLines(filePath);

		if (!lines)
			return true;
	
		if (lineNumber <= 0 || lineNumber > static_cast<int>(lines->size()))
		{
			if (enableLog_)
				LOG_DEBUG("{} line {} does not exist, skipped", filePath.wstring(), lineNumber);
			return false;
		}

		const auto& line = (*lines)[lineNumber - 1];
		return IsLineSelected(line);
	}

	//-------------------------------------------------------------------------
	bool LineFilter::IsLineSelected(const std::string& line) const
	{
		for (const auto& excludedRegex : excludedLineRegexes_)
		{
			if (excludedRegex.Matches(line))
				return false;
		}

		return true;
	}

	//-------------------------------------------------------------------------
	const std::vector<std::string>* LineFilter::GetLines(
		const std::filesystem::path& path)
	{
		if (path != filePath_)
		{
			mappedFileForFilePath_ = Tools::MappedFile::TryCreate(path);
			if (mappedFileForFilePath_)
				++fileReadCount_;
			filePath_ = path;
		}

		return mappedFileForFilePath_ ? &mappedFileForFilePath_->GetLines() : nullptr;
	}

	//-------------------------------------------------------------------------
	int LineFilter::GetFileReadCount() const
	{
		return fileReadCount_;
	}
}
