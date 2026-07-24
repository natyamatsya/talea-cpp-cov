// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2016 OpenCppCoverage

#pragma once
#include <cstdint>

#include <filesystem>
#include "FileFilterExport.hpp"
#include "tools/Error.hpp"

namespace FileFilter
{
	class File;

	class FILEFILTER_DLL UnifiedDiffParser
	{
	public:
		const static std::wstring FromFilePrefix;
		const static std::wstring ToFilePrefix;

		// The error messages reported (by value, as a Tools::Error) for a
		// malformed diff. Public because the parser's tests assert on them.
		const static std::wstring ErrorContextHunks;
		const static std::wstring ErrorNoFilenameBeforeHunks;
		const static std::wstring ErrorCannotReadLine;
		const static std::wstring ErrorExpectFromFilePrefix;
		const static std::wstring ErrorInvalidHunks;

		UnifiedDiffParser() = default;

		[[nodiscard]] Tools::Expected<std::vector<File>> Parse(std::wistream&) const;

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

