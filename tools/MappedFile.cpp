// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2017 OpenCppCoverage

#include "stdafx.h"
#include "MappedFile.hpp"

#include "tools/Tool.hpp"

#include <system_error>
#include <mio/mmap.hpp>

namespace Tools
{
	//-------------------------------------------------------------------------
	MappedFile::MappedFile(std::vector<std::string> lines)
		: lines_{std::move(lines)}
	{
	}

	//-------------------------------------------------------------------------
	Expected<std::vector<std::string>>
	MappedFile::LoadLines(const std::filesystem::path& path)
	{
		// TryCreate guarantees the file exists and is non-empty, so mio does
		// not need to handle those error cases here.
		std::error_code error;
		mio::mmap_source mappedFile = mio::make_mmap_source(path.string(), error);

		if (error)
			return std::unexpected(
			    MAKE_ERROR(L"Cannot create mapped file: " + path.wstring()));

		std::vector<std::string> lines;
		auto begin = mappedFile.begin();
		const auto end = mappedFile.end();
		for (auto it = begin; it != end; ++it)
		{
			if (*it == '\n')
			{
				const auto endOfLine = (it != begin && *(it - 1) == '\r') ? it - 1 : it;
				lines.push_back({ begin, endOfLine });
				begin = it + 1;
			}
		}
		if (begin != end)
			lines.push_back({begin, end});
		return lines;
	}

	//-------------------------------------------------------------------------
	const std::vector<std::string>& MappedFile::GetLines() const
	{
		return lines_;
	}

	//-------------------------------------------------------------------------
	Expected<std::unique_ptr<MappedFile>>
	MappedFile::TryCreate(const std::filesystem::path& path)
	{
		if (!FileExists(path) || std::filesystem::file_size(path) == 0)
			return std::unique_ptr<MappedFile>{nullptr};

		auto lines = LoadLines(path);
		if (!lines)
			return std::unexpected(std::move(lines.error()));
		return std::unique_ptr<MappedFile>(new MappedFile{std::move(*lines)});
	}
}
