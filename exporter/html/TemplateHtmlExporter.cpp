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
#include "TemplateHtmlExporter.hpp"

#include <cstdint>
#include <format>
#include <fstream>
#include <filesystem>
#include <unordered_map>
#include "tools/StringUtils.hpp"

#include "CTemplate.hpp"
#include "tools/Tool.hpp"

#include "talea-cpp-cov/CoverageRate.hpp"

#include "tools/Error.hpp"

namespace cov = TaleaCppCov;
namespace fs = std::filesystem;

namespace Exporter
{
	namespace
	{
		//-------------------------------------------------------------------------
		std::string ToString(const std::wstring& str)
		{
			return Tools::ToUtf8String(str);
		}

		//-------------------------------------------------------------------------
		std::string ToHtmlPath(const fs::path& path)
		{
			auto htmlPath = path.wstring();
			Tools::ReplaceAll(htmlPath, std::wstring_view{L"\\"}, std::wstring_view{L"/"});
			return ToString(htmlPath);
		}

		//-------------------------------------------------------------------------
		Tools::Expected<void> WriteContentTo(const std::string& content, const fs::path& path)
		{
			std::ofstream ofs(path.string(), std::ios::binary);

			if (!ofs)
				return std::unexpected(MAKE_ERROR(L"Cannot open file" << path));
			ofs << content;
			ofs.flush();
			return {};
		}

		//-------------------------------------------------------------------------
		Tools::Expected<std::string> GenerateTemplate(
			const ctemplate::TemplateDictionary& templateDictionary,
			const fs::path& templatePath)
		{
			// The parsed template is immutable and identical for every source
			// file, so cache the resolved Template* per thread: each worker
			// resolves it once instead of taking ctemplate's global template
			// -cache mutex on every file. (Parallel HTML export otherwise
			// serializes threads on that mutex per page.)
			thread_local std::unordered_map<std::string, ctemplate::Template*>
				templateCache;

			auto key = templatePath.string();
			auto& cached = templateCache[key];
			if (!cached)
			{
				cached = ctemplate::Template::GetTemplate(
					key, ctemplate::DO_NOT_STRIP);
				if (!cached)
					return std::unexpected(MAKE_ERROR(
					    L"Cannot load template " << templatePath.wstring()));
			}

			std::string output;
			if (!cached->Expand(&output, &templateDictionary))
				return std::unexpected(MAKE_ERROR(
				    L"Cannot generate output for " << templatePath.wstring()));

			return output;
		}
		
		//-------------------------------------------------------------------------
		Tools::Expected<void> WriteTemplate(
			const ctemplate::TemplateDictionary& templateDictionary,
			const fs::path& templatePath,
			const fs::path& output)
		{
			return GenerateTemplate(templateDictionary, templatePath)
			    .and_then([&](const std::string& content) {
				    return WriteContentTo(content, output);
			    });
		}
	}
	
	//-------------------------------------------------------------------------
	const std::string TemplateHtmlExporter::MainTemplateItemSection = "ITEMS";
	const std::string TemplateHtmlExporter::TitleTemplate = "TITLE";
	const std::string TemplateHtmlExporter::ExecutedLineTemplate = "EXECUTED_LINE";
	const std::string TemplateHtmlExporter::UnExecutedLineTemplate = "UNEXECUTED_LINE";
	const std::string TemplateHtmlExporter::LinkTemplate = "LINK";
	const std::string TemplateHtmlExporter::TotalLineTemplate = "TOTAL_LINE";	
	const std::string TemplateHtmlExporter::NameTemplate = "NAME";
	const std::string TemplateHtmlExporter::ItemLinkSection = "ITEM_LINK";
	const std::string TemplateHtmlExporter::ItemNoLinkSection = "ITEM_NO_LINK";
	const std::string TemplateHtmlExporter::ItemSimpleText = "ITEM_SIMPLE_TEXT";
	const std::string TemplateHtmlExporter::BodyOnLoadTemplate = "BODY_ON_LOAD";
	const std::string TemplateHtmlExporter::SourceWarningMessageTemplate = "SOURCE_WARNING_MESSAGE";
	const std::string TemplateHtmlExporter::BodyOnLoadFct = "prettyPrint()";
	const std::string TemplateHtmlExporter::SyntaxHighlightingDisabledMsg 
		= "Syntax highlighting has been disabled for performance reasons.";
	const std::string TemplateHtmlExporter::MainMessageTemplate = "MAIN_MESSAGE";
	const std::string TemplateHtmlExporter::CoverRateTemplate = "COVER_RATE";
	const std::string TemplateHtmlExporter::UncoverRateTemplate = "UNCOVER_RATE";
	const std::string TemplateHtmlExporter::CodeTemplate = "CODE";
	const std::string TemplateHtmlExporter::IdTemplate = "ID";
	const std::string TemplateHtmlExporter::ThirdPartyPathTemplate = "THIRD_PARTY_PATH";
	const std::string TemplateHtmlExporter::TaleaProjectLink = "TALEA_PROJECT_LINK";
	const std::string TemplateHtmlExporter::TaleaVersion = "TALEA_VERSION";
	const std::string TemplateHtmlExporter::ActualProjectLink = "https://github.com/OpenCppCoverage/OpenCppCoverage/releases";

	//-------------------------------------------------------------------------
	TemplateHtmlExporter::TemplateHtmlExporter(
		const fs::path& mainTemplatePath,
		const fs::path& fileTemplatePath)
		: mainTemplatePath_(mainTemplatePath)
		, fileTemplatePath_(fileTemplatePath)
	{		
	}

	//-------------------------------------------------------------------------
	std::unique_ptr<ctemplate::TemplateDictionary> 
	TemplateHtmlExporter::CreateTemplateDictionary(
		const std::wstring& title, 
		const std::wstring& message) const
	{
		auto titleStr = ToString(title);
		std::unique_ptr<ctemplate::TemplateDictionary> dictionary;

		dictionary.reset(new ctemplate::TemplateDictionary(titleStr));

		dictionary->SetValue(TitleTemplate, titleStr);
		dictionary->SetValue(MainMessageTemplate, ToString(message));
		dictionary->SetValue(TaleaProjectLink, ActualProjectLink);
		dictionary->SetValue(TaleaVersion, TALEA_CPP_COV_VERSION);

		return dictionary;
	}

	//-------------------------------------------------------------------------
	void TemplateHtmlExporter::AddFileSectionToDictionary(
		const fs::path& originalFilename,
		const cov::CoverageRate& coverageRate,
		bool isSimpleText,
		const fs::path* fileOutput,		
		ctemplate::TemplateDictionary& moduleTemplateDictionary)
	{
		auto sectionDictionary = moduleTemplateDictionary.AddSectionDictionary(MainTemplateItemSection);
		
		moduleTemplateDictionary.SetValue(ThirdPartyPathTemplate, "../third-party");
		FillSection(*sectionDictionary, isSimpleText, fileOutput, coverageRate, originalFilename);
	}
	
	//-------------------------------------------------------------------------
	void TemplateHtmlExporter::AddModuleSectionToDictionary(			
		const fs::path& originalFilename,
		const cov::CoverageRate& coverageRate,
		bool isSimpleText,
		const fs::path* moduleOutput,
		ctemplate::TemplateDictionary& projectDictionary)
	{
		auto sectionDictionary = projectDictionary.AddSectionDictionary(MainTemplateItemSection);			
			
		projectDictionary.SetValue(ThirdPartyPathTemplate, "third-party");
		FillSection(*sectionDictionary, isSimpleText, moduleOutput, coverageRate, originalFilename);
	}				
	
	//-------------------------------------------------------------------------
	void TemplateHtmlExporter::GenerateModuleTemplate(
		const ctemplate::TemplateDictionary& templateDictionary,
		const fs::path& output) const
	{
		Tools::ThrowOnError(
		    WriteTemplate(templateDictionary, mainTemplatePath_, output));
	}

	//-------------------------------------------------------------------------
	void TemplateHtmlExporter::GenerateProjectTemplate(
		const ctemplate::TemplateDictionary& templateDictionary,
		const fs::path& output) const
	{
		Tools::ThrowOnError(
		    WriteTemplate(templateDictionary, mainTemplatePath_, output));
	}

	//-------------------------------------------------------------------------
	void TemplateHtmlExporter::GenerateSourceTemplate(
		const std::wstring& title,
		const std::wstring& codeContent,
		bool enableCodePrettify,
		const fs::path& output) const
	{
		auto titleStr = ToString(title);
		ctemplate::TemplateDictionary dictionary(titleStr);
		std::string bodyLoad = BodyOnLoadFct;
		std::string warning = "";

		if (!enableCodePrettify)
		{
			bodyLoad = "";
			warning = SyntaxHighlightingDisabledMsg;
		}

		dictionary.SetValue(TitleTemplate, titleStr);
		dictionary.SetValue(CodeTemplate, ToString(codeContent));
		dictionary.SetValue(BodyOnLoadTemplate, bodyLoad);
		dictionary.SetValue(SourceWarningMessageTemplate, warning);
		dictionary.SetValue(TaleaProjectLink, ActualProjectLink);
		dictionary.SetValue(TaleaVersion, TALEA_CPP_COV_VERSION);
		Tools::ThrowOnError(WriteTemplate(dictionary, fileTemplatePath_, output));
	}
	//-------------------------------------------------------------------------
	std::string TemplateHtmlExporter::GetUuid()
	{
		// A UUID-shaped hex id, unique within the report. Two 64-bit draws give
		// the 128 bits; the exact layout is irrelevant (it is only an HTML id).
		const std::uint64_t high = uuidGenerator_();
		const std::uint64_t low = uuidGenerator_();
		return std::format(
		    "{:08x}-{:04x}-{:04x}-{:04x}-{:012x}",
		    static_cast<std::uint32_t>(high >> 32),
		    static_cast<std::uint16_t>(high >> 16),
		    static_cast<std::uint16_t>(high),
		    static_cast<std::uint16_t>(low >> 48),
		    low & 0xFFFFFFFFFFFFULL);
	}

	//-------------------------------------------------------------------------
	void TemplateHtmlExporter::FillSection(
		ctemplate::TemplateDictionary& sectionDictionary,
		bool isSimpleText,
		const fs::path* link,
		const cov::CoverageRate& coverageRate,
		const fs::path& originalFilename)
	{
		if (link)
		{
			auto htmlPath = ToHtmlPath(*link);
			sectionDictionary.SetValueAndShowSection(
				LinkTemplate, 
				htmlPath, 
				ItemLinkSection);
		}
		else
		{
			sectionDictionary.ShowSection(isSimpleText ? ItemSimpleText
			                                           : ItemNoLinkSection);
		}

		sectionDictionary.SetIntValue(CoverRateTemplate, coverageRate.GetPercentRate());
		sectionDictionary.SetIntValue(UncoverRateTemplate, 100 - coverageRate.GetPercentRate());
		sectionDictionary.SetIntValue(ExecutedLineTemplate, coverageRate.GetExecutedLinesCount());
		sectionDictionary.SetIntValue(UnExecutedLineTemplate, coverageRate.GetUnExecutedLinesCount());
		sectionDictionary.SetIntValue(TotalLineTemplate, coverageRate.GetTotalLinesCount());
		sectionDictionary.SetValue(IdTemplate, GetUuid());
		auto name = ToString(originalFilename.wstring());
		sectionDictionary.SetValue(NameTemplate, name);
	}
}
