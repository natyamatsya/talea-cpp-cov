// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2014 OpenCppCoverage

#pragma once

#include <filesystem>
#include "../ExporterExport.hpp"

#include "TemplateHtmlExporter.hpp"
#include "HtmlFileCoverageExporter.hpp"
#include "../IExporter.hpp"

namespace Coverage
{
	class CoverageData;
	class FileCoverage;
	class ModuleCoverage;
}

namespace TaleaCppCov
{
	class CoverageRateComputer;
}

namespace Exporter
{
	class HtmlFolderStructure;
	class HtmlFile;

	class EXPORTER_DLL HtmlExporter: public IExporter
	{
	public:
		static const std::wstring WarningExitCodeMessage;

	public:
		explicit HtmlExporter(const std::filesystem::path& templateFolder);

		std::filesystem::path GetDefaultPath(const std::wstring& prefix) const override;
		void Export(const Coverage::CoverageData&, const std::filesystem::path& outputFolder) override;

	private:
		HtmlExporter(const HtmlExporter&) = delete;
		HtmlExporter& operator=(const HtmlExporter&) = delete;

		// Reads a source file, syntax-highlights it, and writes its HTML page
		// to htmlFile's (already unique) output path. It touches only immutable
		// state and a distinct output file, so it is safe to parallelize;
		// generation is currently run serially (see ExportFiles).
		std::optional<std::filesystem::path> GenerateFileHtml(
			const Coverage::FileCoverage& fileCoverage,
			const HtmlFile& htmlFile) const;

		void ExportFiles(
			TaleaCppCov::CoverageRateComputer&,
			const Coverage::ModuleCoverage& module,
			const HtmlFolderStructure& htmlFolderStructure,
			ctemplate::TemplateDictionary& moduleTemplateDictionary);

	private:
		TemplateHtmlExporter exporter_;
		HtmlFileCoverageExporter fileCoverageExporter_;
		std::filesystem::path templateFolder_;
	};
}


