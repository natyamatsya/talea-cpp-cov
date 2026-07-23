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

#include "stdafx.h"
#include "HtmlExporter.hpp"

#include <format>
#include <optional>
#include <sstream>
#include <iomanip>
#include "CTemplate.hpp"

#include "Coverage/CoverageData.hpp"
#include "Coverage/ModuleCoverage.hpp"
#include "Coverage/FileCoverage.hpp"
#include "CppCoverage/CoverageRateComputer.hpp"
#include "CppCoverage/CoverageRate.hpp"

#include "Tools/Log.hpp"
#include "Tools/Tool.hpp"

#include "TemplateHtmlExporter.hpp"
#include "HtmlFileCoverageExporter.hpp"
#include "HtmlFolderStructure.hpp"
#include "HtmlFile.hpp"
namespace cov = CppCoverage;

namespace Exporter
{		

	namespace
	{
		//-------------------------------------------------------------------------
		std::wstring GetMainMessage(const Coverage::CoverageData& coverageData)
		{
			auto exitCode = coverageData.GetExitCode();

			if (exitCode)
				return std::format(L"{}{}",
				                   HtmlExporter::WarningExitCodeMessage,
				                   exitCode);
			return L"";
		}
	}
	
	//-------------------------------------------------------------------------
	const std::wstring HtmlExporter::WarningExitCodeMessage = L"Warning: Your program has exited with error code: ";

	//-------------------------------------------------------------------------
	HtmlExporter::HtmlExporter(const fs::path& templateFolder)
		: exporter_(templateFolder / "MainTemplate.html", templateFolder / "SourceTemplate.html")
		, fileCoverageExporter_()
		, templateFolder_(templateFolder)
	{
	}

	//-------------------------------------------------------------------------
	std::filesystem::path HtmlExporter::GetDefaultPath(const std::wstring&) const
	{
		auto now = std::time(nullptr);
		auto localNow = std::localtime(&now);
		std::ostringstream ostr;

		ostr << "CoverageReport-" << std::put_time(localNow, "%Y-%m-%d-%Hh%Mm%Ss");

		return ostr.str();
	}

	//-------------------------------------------------------------------------
	void HtmlExporter::Export(
		const Coverage::CoverageData& coverageData, 
		const std::filesystem::path& outputFolderPrefix)
	{	
		HtmlFolderStructure htmlFolderStructure{templateFolder_};
		cov::CoverageRateComputer coverageRateComputer{ coverageData };

		auto mainMessage = GetMainMessage(coverageData);

		auto projectDictionary = exporter_.CreateTemplateDictionary(coverageData.GetName(), mainMessage);
		auto outputFolder = htmlFolderStructure.CreateCurrentRoot(outputFolderPrefix);

		exporter_.AddModuleSectionToDictionary(
		    coverageData.GetName(),
		    coverageRateComputer.GetCoverageRate(),
			true,
			nullptr,
		    *projectDictionary);

		for (const auto& module : coverageRateComputer.SortModulesByCoverageRate())
		{			
			const auto& moduleCoverageRate = coverageRateComputer.GetCoverageRate(*module);

			if (moduleCoverageRate.GetTotalLinesCount())
			{
				const auto& modulePath = module->GetPath();
				auto moduleFilename = module->GetPath().filename();
				auto moduleTemplateDictionary = exporter_.CreateTemplateDictionary(moduleFilename.wstring(), L"");

				auto htmlModulePath = Tools::ValueOrThrow(
				    htmlFolderStructure.CreateCurrentModule(modulePath));
				ExportFiles(coverageRateComputer, *module, htmlFolderStructure, *moduleTemplateDictionary);

				exporter_.GenerateModuleTemplate(*moduleTemplateDictionary, htmlModulePath.GetAbsolutePath());
				exporter_.AddModuleSectionToDictionary(
				    module->GetPath(),
				    moduleCoverageRate,
					false,
				    &htmlModulePath.GetRelativeLinkPath(),
				    *projectDictionary);
			}
		}

		exporter_.GenerateProjectTemplate(*projectDictionary, outputFolder / L"index.html");
		Tools::ShowOutputMessage(L"Coverage generated in Folder ", outputFolder);
	}	

	//---------------------------------------------------------------------
	void HtmlExporter::ExportFiles(
		cov::CoverageRateComputer& coverageRateComputer,
		const Coverage::ModuleCoverage& module,
		const HtmlFolderStructure& htmlFolderStructure,
		ctemplate::TemplateDictionary& moduleTemplateDictionary)
	{
		exporter_.AddFileSectionToDictionary(
			module.GetPath(),
			coverageRateComputer.GetCoverageRate(module),
			true,
			nullptr,
			moduleTemplateDictionary);

		// HTML generation is dominated by string allocation (the page buffers)
		// and disk writes, not CPU, so parallelizing the per-file loop was
		// measured to be a wash to a slight regression (heap-lock contention
		// outweighs any highlighting speedup even on many cores). It stays
		// serial; the win is the thread-local template cache in
		// TemplateHtmlExporter, which avoids ctemplate's global cache mutex on
		// every file.
		for (const auto* file : coverageRateComputer.SortFilesByCoverageRate(module))
		{
			auto htmlFile = Tools::ValueOrThrow(
			    htmlFolderStructure.GetHtmlFilePath(file->GetPath()));
			auto link = GenerateFileHtml(*file, htmlFile);
			exporter_.AddFileSectionToDictionary(
				file->GetPath(),
				coverageRateComputer.GetCoverageRate(*file),
				false,
				link ? &*link : nullptr,
				moduleTemplateDictionary);
		}
	}

	//---------------------------------------------------------------------
	std::optional<fs::path> HtmlExporter::GenerateFileHtml(
		const Coverage::FileCoverage& fileCoverage,
		const HtmlFile& htmlFile) const
	{
		if (!Tools::FileExists(fileCoverage.GetPath()))
			return std::optional<fs::path>();

		std::wostringstream ostr;
		auto enableCodePrettify =
		    Tools::ValueOrThrow(fileCoverageExporter_.Export(fileCoverage, ostr));

		auto title = fileCoverage.GetPath().filename().wstring();
		exporter_.GenerateSourceTemplate(
			title, ostr.str(), enableCodePrettify, htmlFile.GetAbsolutePath());

		return htmlFile.GetRelativeLinkPath();
	}
}

