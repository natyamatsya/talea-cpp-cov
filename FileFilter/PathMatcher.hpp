// OpenCppCoverage is an open source code coverage for C++.
// Copyright (C) 2016 OpenCppCoverage

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
		std::expected<File*, AmbiguousPath> Match(const std::filesystem::path&);

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

