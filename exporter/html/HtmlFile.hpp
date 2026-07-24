// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2014 OpenCppCoverage

#pragma once

#include <filesystem>

#include "../ExporterExport.hpp"

namespace Exporter
{
	class EXPORTER_DLL HtmlFile
	{
	public:
		HtmlFile(
			std::filesystem::path absolutePath,
			std::filesystem::path relativeLinkPath);
		HtmlFile(const HtmlFile&) = default;

		const std::filesystem::path& GetAbsolutePath() const;
		const std::filesystem::path& GetRelativeLinkPath() const;

	private:		
		HtmlFile& operator=(const HtmlFile&) = delete;

		std::filesystem::path absolutePath_;
		std::filesystem::path relativeLinkPath_;
	};
}

