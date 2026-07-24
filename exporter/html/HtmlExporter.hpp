// OpenCppCoverage is an open source code coverage for C++.
// Copyright (C) 2014 OpenCppCoverage
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


