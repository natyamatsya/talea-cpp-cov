// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2016 OpenCppCoverage

#pragma once

#include "FileFilterExport.hpp"
#include <vector>
#include <memory>
#include <filesystem>
#include <optional>
#include <expected>

namespace FileFilter
{
	class File;

	// The three paths that made a filename-postfix match ambiguous: the shared
	// postfix and the two distinct full paths that both end with it. Carried as
	// the error of PathMatcher::Match (replaces the former AmbiguousPathException,
	// which was the only custom exception carrying structured data).
	struct AmbiguousPath
	{
		std::filesystem::path postFixPath;
		std::filesystem::path firstPossiblePath;
		std::filesystem::path secondPossiblePath;
	};

	class FILEFILTER_DLL PathMatcher
	{
	public:
		explicit PathMatcher(
			std::vector<File>&&,
			const std::optional<std::filesystem::path>& parentPath);
		~PathMatcher();

		// The matched File (nullptr when nothing matches), or the ambiguous
		// paths when one filename postfix matches two different files.
		[[nodiscard]] std::expected<File*, AmbiguousPath> Match(const std::filesystem::path&);

		using PathCollection = std::vector<std::filesystem::path>;
		PathCollection GetUnmatchedPaths() const;

	private:
		PathMatcher(const PathMatcher&) = delete;
		PathMatcher& operator=(const PathMatcher&) = delete;
		PathMatcher(PathMatcher&&) = delete;
		PathMatcher& operator=(PathMatcher&&) = delete;

		class IPathMatcherEngine;
		class PostFixPathMatcherEngine;
		class FullPathMatcherEngine;

		std::unique_ptr<IPathMatcherEngine> pathMatcherEngine_;
	};
}

