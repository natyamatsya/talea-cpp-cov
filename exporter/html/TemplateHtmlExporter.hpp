// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2014 OpenCppCoverage

#pragma once

#include <memory>

#include <filesystem>
#include <random>

#include "../ExporterExport.hpp"

namespace TaleaCppCov
{
	class CoverageRate;
}

namespace ctemplate
{
	class TemplateDictionary;
}

namespace fs = std::filesystem;

namespace Exporter
{
	class ITemplateExpander;

	class EXPORTER_DLL TemplateHtmlExporter
	{
	public:
		static const std::string ExecutedLineTemplate;
		static const std::string TitleTemplate;
		static const std::string MainTemplateItemSection;
		static const std::string UnExecutedLineTemplate;
		static const std::string LinkTemplate;
		static const std::string TotalLineTemplate;		
		static const std::string NameTemplate;
		static const std::string ItemLinkSection;
		static const std::string ItemNoLinkSection;
		static const std::string ItemSimpleText;
		static const std::string BodyOnLoadTemplate;
		static const std::string SourceWarningMessageTemplate;
		static const std::string BodyOnLoadFct;
		static const std::string SyntaxHighlightingDisabledMsg;
		static const std::string MainMessageTemplate;
		static const std::string CoverRateTemplate;
		static const std::string UncoverRateTemplate;
		static const std::string CodeTemplate;
		static const std::string IdTemplate;
		static const std::string ThirdPartyPathTemplate;
		static const std::string TaleaProjectLink;
		static const std::string TaleaVersion;
		static const std::string ActualProjectLink;

	public:
		explicit TemplateHtmlExporter(
			const fs::path& mainTemplatePath,
			const fs::path& fileTemplatePath);

		std::unique_ptr<ctemplate::TemplateDictionary>	
		CreateTemplateDictionary(const std::wstring& title, const std::wstring& message) const;

		void AddFileSectionToDictionary(
			const fs::path& originalFilename,
			const TaleaCppCov::CoverageRate& coverageRate,
			bool isSimpleText,
			const fs::path* fileOutput,
			ctemplate::TemplateDictionary& moduleTemplateDictionary);

		void AddModuleSectionToDictionary(
			const fs::path& originalFilename,
			const TaleaCppCov::CoverageRate& coverageRate,
			bool isSimpleText,
			const fs::path* moduleOutput,
			ctemplate::TemplateDictionary& projectDictionary);

		void GenerateModuleTemplate(
			const ctemplate::TemplateDictionary& templateDictionary,
			const fs::path&) const;

		void GenerateProjectTemplate(
			const ctemplate::TemplateDictionary& templateDictionary,
			const fs::path&) const;

		void GenerateSourceTemplate(
			const std::wstring& title, 
			const std::wstring& codeContent,
			bool enableCodePrettify,
			const fs::path& output) const;

	private:
		TemplateHtmlExporter(const TemplateHtmlExporter&) = delete;
		TemplateHtmlExporter& operator=(const TemplateHtmlExporter&) = delete;
		std::string GetUuid();
		void FillSection(
			ctemplate::TemplateDictionary&,
			bool isSimpleText,
			const fs::path* link,
			const TaleaCppCov::CoverageRate&,
			const fs::path& originalFilename);

	private:
		fs::path mainTemplatePath_;
		fs::path fileTemplatePath_;
		// Generates the unique id used for each HTML section; the value only
		// needs to be unique within a report, so a seeded PRNG replaces the
		// former boost::uuids dependency.
		std::mt19937_64 uuidGenerator_{std::random_device{}()};
	};
}


