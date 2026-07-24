// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2016 OpenCppCoverage

#include "stdafx.h"
#include <cstdint>

#include <fstream>
#include <sstream>
#include <optional>
#include "tools/Log.hpp"
#include "UnifiedDiffCoverageFilter.hpp"
#include "UnifiedDiffParser.hpp"
#include "File.hpp"

namespace FileFilter
{
	namespace
	{
		//---------------------------------------------------------------------
		std::vector<File> ParseUnifiedDiff(const std::filesystem::path& unifiedDiffPath)
		{
			std::wifstream ifs(unifiedDiffPath.wstring());

			if (!ifs)
				throw std::runtime_error("The file " + unifiedDiffPath.string() + " cannot be opened.");

			auto files = Tools::ValueOrThrow(UnifiedDiffParser{}.Parse(ifs));
			LOG_DEBUG("Unified diff: {}", unifiedDiffPath);
			for (const auto& file : files)
			{
				LOG_DEBUG("Selected lines for {}: ", file.GetPath().wstring());
				LOG_DEBUG("{}", Tools::Join(file.GetSelectedLines()));
			}
			return files;
		}
	}

	//-------------------------------------------------------------------------
	UnifiedDiffCoverageFilter::UnifiedDiffCoverageFilter(
		const std::filesystem::path& unifiedDiffPath,
		const std::optional<std::filesystem::path>& rootDiffFolder)
		: UnifiedDiffCoverageFilter{ ParseUnifiedDiff(unifiedDiffPath), rootDiffFolder }
	{
	}

	//-------------------------------------------------------------------------
	UnifiedDiffCoverageFilter::UnifiedDiffCoverageFilter(
		std::vector<File>&& files,
		const std::optional<std::filesystem::path>& rootDiffFolder)
		: lastFile_{ nullptr }
		, pathMatcher_{ std::move(files), rootDiffFolder }
	{
	}

	//-------------------------------------------------------------------------
	bool UnifiedDiffCoverageFilter::IsSourceFileSelected(const std::filesystem::path& path)
	{
		return SearchFile(path) != nullptr;
	}

	//-------------------------------------------------------------------------
	bool UnifiedDiffCoverageFilter::IsLineSelected(const std::filesystem::path& path, std::int32_t lineNumber)
	{
		auto file = SearchFile(path);

		if (!file)
			return false;

		return file->IsLineSelected(lineNumber);
	}

	//-------------------------------------------------------------------------
	File* UnifiedDiffCoverageFilter::SearchFile(const std::filesystem::path& path)
	{
		if (path == lastPath_)
			return lastFile_;

		auto file = pathMatcher_.Match(path);
		if (!file)
		{
			const auto& ambiguous = file.error();
			std::ostringstream ostr;

			ostr << "A path is ambiguous in the unified diff file." << std::endl;
			ostr << ambiguous.postFixPath << " can be either "
			     << ambiguous.firstPossiblePath;
			ostr << " or " << ambiguous.secondPossiblePath << std::endl;
			ostr << "Please specify root folder. See help for more information.";

			throw std::runtime_error(ostr.str());
		}

		lastFile_ = *file;
		lastPath_ = path;
		return lastFile_;
	}

	//-------------------------------------------------------------------------
	std::vector<std::filesystem::path> UnifiedDiffCoverageFilter::GetUnmatchedPaths() const
	{
		return pathMatcher_.GetUnmatchedPaths();
	}
}
