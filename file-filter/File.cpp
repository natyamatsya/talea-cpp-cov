// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2016 OpenCppCoverage

#include "stdafx.h"
#include <cstdint>
#include "File.hpp"

namespace FileFilter
{
	//----------------------------------------------------------------------------
	File::File(const std::filesystem::path& path)
		: path_{path}
	{
	}

	//----------------------------------------------------------------------------
	void File::AddSelectedLines(const std::vector<std::int32_t>& lines)
	{
		selectedLines_.insert(lines.begin(), lines.end());
	}

	//----------------------------------------------------------------------------
	const std::filesystem::path& File::GetPath() const
	{
		return path_;
	}

	//----------------------------------------------------------------------------
	void File::SetPath(const std::filesystem::path& path)
	{
		path_ = path;
	}

	//----------------------------------------------------------------------------
	const std::set<std::int32_t>& File::GetSelectedLines() const
	{
		return selectedLines_;
	}

	//----------------------------------------------------------------------------
	bool File::IsLineSelected(std::int32_t lineNumber) const
	{
		return selectedLines_.find(lineNumber) != selectedLines_.end();
	}

	//----------------------------------------------------------------------------
	bool File::operator==(const File& other) const
	{
		return path_ == other.path_
			&& selectedLines_ == other.selectedLines_;
	}
}
