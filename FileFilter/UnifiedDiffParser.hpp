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
#include <cstdint>

#include <filesystem>
#include "FileFilterExport.hpp"
#include "Tools/Error.hpp"

namespace FileFilter
{
	class File;

	class FILEFILTER_DLL UnifiedDiffParser
	{
	public:
		const static std::wstring FromFilePrefix;
		const static std::wstring ToFilePrefix;

		UnifiedDiffParser() = default;

		Tools::Expected<std::vector<File>> Parse(std::wistream&) const;

	private:
		UnifiedDiffParser(const UnifiedDiffParser&) = delete;
		UnifiedDiffParser& operator=(const UnifiedDiffParser&) = delete;
		UnifiedDiffParser(UnifiedDiffParser&&) = delete;
		UnifiedDiffParser& operator=(UnifiedDiffParser&&) = delete;

		struct HunksDifferences;
		struct Stream;

		Tools::Expected<std::filesystem::path> ExtractTargetFile(Stream&) const;
		Tools::Expected<HunksDifferences> ExtractHunksDifferences(
							const Stream&,
							const std::wstring& hunksDifferencesLine) const;

		Tools::Expected<std::vector<std::int32_t>> ExtractUpdatedLines(
							Stream&,
							const std::wstring& hunksDifferencesLine) const;

		// Builds (does not throw) the "Error line N: <line>\n<message>" error
		// that the helpers return as std::unexpected.
		Tools::Error MakeError(const Stream&, const std::wstring&) const;

		Tools::Expected<void> FillUpdatedLines(
			const std::wstring& line,
			std::vector<File>& files,
			Stream& stream) const;
	};
}

