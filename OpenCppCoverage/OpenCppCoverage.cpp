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
#include <cstdint>
#include "OpenCppCoverage.hpp"

#include <iostream>
#include <fstream>
#include <optional>

#include "CppCoverage/CodeCoverageRunner.hpp"
#include "CppCoverage/VehCoverageEngine.hpp"
#include "CppCoverage/FastCoverageImporter.hpp"
#include "CppCoverage/CoverageFilterSettings.hpp"
#include "CppCoverage/OptionsParser.hpp"
#include "CppCoverage/Options.hpp"
#include "CppCoverage/ProgramOptions.hpp"
#include "CppCoverage/CoverageDataMerger.hpp"
#include "CppCoverage/OptionsExport.hpp"
#include "CppCoverage/RunCoverageSettings.hpp"
#include "CppCoverage/ExportOptionParser.hpp"

#include "Exporter/Html/HtmlExporter.hpp"
#include "Exporter/CoberturaExporter.hpp"
#include "Exporter/Binary/BinaryExporter.hpp"
#include "Exporter/Binary/CoverageDataDeserializer.hpp"
#include "Exporter/Binary/ColumnarCoverage.hpp"

#include "Tools/Tool.hpp"
#include "Tools/Log.hpp"
#include "Tools/WarningManager.hpp"

namespace cov = CppCoverage;

namespace OpenCppCoverage
{
	namespace
	{
		//-----------------------------------------------------------------------------
		std::wstring GetDefaultPathPrefix(const cov::Options& options)
		{
			const auto* startInfo = options.GetStartInfo();

			if (startInfo)
			{
				auto path = startInfo->GetPath();
				fs::path runningCommandFilenamePath = path.filename().replace_extension("");
				
				return runningCommandFilenamePath.wstring();
			}

			return L"CoverageOutput";
		}

		//-------------------------------------------------------------------------
		std::filesystem::path GetTemplateFolder()
		{
			return Tools::ValueOrThrow(Tools::GetExecutableFolder()) / "Template";
		}

		//-----------------------------------------------------------------------------
		void
		Export(const cov::Options& options,
		       const Coverage::CoverageData& coverage)
		{
			const auto& exports = options.GetExports();
			std::map<cov::OptionsExportType, std::unique_ptr<Exporter::IExporter>> exporters;
			
			exporters.emplace(cov::OptionsExportType::Html, 
				std::unique_ptr<Exporter::IExporter>(std::make_unique<Exporter::HtmlExporter>( GetTemplateFolder() )));
			exporters.emplace(cov::OptionsExportType::Cobertura, 
				std::unique_ptr<Exporter::IExporter>(std::make_unique<Exporter::CoberturaExporter>()));
			exporters.emplace(cov::OptionsExportType::Binary,
				std::unique_ptr<Exporter::IExporter>(std::make_unique<Exporter::BinaryExporter>()));
			
			auto defaultPathPrefix = GetDefaultPathPrefix(options);

			// Collapse per-module duplicates of the same source file (union of
			// their coverage) for the COBERTURA export only. That report is
			// the CI-ingested artifact, where statically-linked shared code
			// compiled into many binaries otherwise appears once per binary -
			// inflating both the percentages and the file size (on a
			// test-suite build tree, by the executable count). The HTML report
			// keeps its per-module structure, which is what makes it navigable
			// and lets --modules / --excluded_modules stay visible in it.
			// Built lazily and once, shared across matching exports.
			bool aggregateByFile = options.IsAggregateByFileModeEnabled();
			std::optional<Coverage::CoverageData> aggregatedByFile;

			for (const auto& singleExport : exports)
			{
				auto exportType = singleExport.GetType();
				auto parameter = singleExport.GetParameter();

				const auto& exporter = exporters.at(exportType);
				auto output =
				    (parameter)
				        ? fs::path{*parameter}
				        : exporter->GetDefaultPath(defaultPathPrefix);

				const Coverage::CoverageData* data = &coverage;
				if (aggregateByFile &&
				    exportType == cov::OptionsExportType::Cobertura)
				{
					if (!aggregatedByFile)
						aggregatedByFile =
						    cov::CoverageDataMerger{}.AggregateByFile(coverage);
					data = &*aggregatedByFile;
				}

				exporter->Export(*data, output);
			}
		}

		//-----------------------------------------------------------------------------
		// Loads every --input_coverage file into the columnar merger. The files
		// are the columnar v2 format the binary exporter writes, read straight
		// into the accumulator and OR-ed together (no intermediate CoverageData
		// per shard). A file that is not columnar is rejected by the
		// deserializer.
		void LoadInputCoverageDatas(
		    const cov::Options& options,
		    Exporter::ColumnarCoverageMerger& merger)
		{
			Exporter::CoverageDataDeserializer coverageDataDeserializer;

			for (const auto& path : options.GetInputCoveragePaths())
			{
				LOG_INFO("Load coverage file: {}", path.wstring());
				if (!merger.AddColumnarFile(path))
				{
					auto errorMsg =
					    "Cannot extract coverage data from " + path.string();
					merger.Add(Tools::ValueOrThrow(
					    coverageDataDeserializer.Deserialize(path, errorMsg)));
				}
			}
		}

		//-----------------------------------------------------------------------------
		// Imports every --input_fastcov dump, appends its coverage to
		// coverageDatas, and writes the combined per-template report when asked.
		void LoadFastCovCoverageDatas(
		    const cov::Options& options,
		    std::vector<Coverage::CoverageData>& coverageDatas)
		{
			const auto& fastCovPaths = options.GetInputFastCovPaths();
			if (fastCovPaths.empty())
			{
				if (options.GetFastCovTemplateReportPath())
					LOG_WARNING("--fastcov_template_report has no effect without --input_fastcov; no report was written.");
				return;
			}

			cov::CoverageFilterSettings coverageFilterSettings{
			    options.GetModulePatterns(), options.GetSourcePatterns()};

			std::vector<cov::FastCoverageImporter::TemplateReport> templates;
			cov::FastCoverageImporter importer{
			    coverageFilterSettings,
			    options.GetUnifiedDiffSettingsCollection(),
			    options.GetExcludedLineRegexes(),
			    options.GetSubstitutePdbSourcePaths()};

			for (const auto& path : fastCovPaths)
			{
				LOG_INFO("Import fast coverage dump: {}", path.wstring());
				auto result = Tools::ValueOrThrow(importer.Import(path));
				coverageDatas.push_back(std::move(result.coverageData_));
				for (auto& report : result.templates_)
					templates.push_back(std::move(report));
			}

			const auto& reportPath = options.GetFastCovTemplateReportPath();
			if (reportPath)
			{
				auto markdown =
				    cov::FastCoverageImporter::FormatTemplateReport(templates);
				std::ofstream ofs{*reportPath, std::ios::binary};
				ofs << Tools::ToUtf8String(markdown);
				LOG_INFO("Wrote template coverage report: {}", reportPath->wstring());
			}
		}

		//-----------------------------------------------------------------------------
		void InitLogger(const cov::Options& options)
		{
			auto logLevel = Tools::LogLevel::Info;

			switch (options.GetLogLevel())
			{
				case cov::LogLevel::Verbose: logLevel = Tools::LogLevel::Debug; break;
				case cov::LogLevel::Quiet: logLevel = Tools::LogLevel::Error; break;
				case cov::LogLevel::Normal: break;
			}

			Tools::InitConsoleAndFileLog(L"LastCoverageResults.log");
			Tools::SetLoggerMinSeverity(logLevel);
		}

		//-----------------------------------------------------------------------------
		std::int32_t Run(const cov::Options& options,
		        std::shared_ptr<Tools::WarningManager> warningManager)
		{
			InitLogger(options);

			// The merger accumulates input shards first, then fastcov imports,
			// then the live run below - the same order the old vector-based
			// merge saw, so the merged name/exit-code stay identical.
			Exporter::ColumnarCoverageMerger merger;
			LoadInputCoverageDatas(options, merger);

			std::vector<Coverage::CoverageData> coveraDatas;
			LoadFastCovCoverageDatas(options, coveraDatas);
			const auto* startInfo = options.GetStartInfo();
			
			std::wostringstream ostr;
			ostr << std::endl << options;
			LOG_INFO("Start Program:{}", ostr.str());

			cov::CodeCoverageRunner codeCoverageRunner{ warningManager };
			cov::CoverageFilterSettings coverageFilterSettings{ options.GetModulePatterns(), options.GetSourcePatterns() };
			auto exitCode = 0;

			if (startInfo)
			{
				size_t maxUnmatchPathsForWarning = (options.GetLogLevel() == cov::LogLevel::Verbose) 
					? std::numeric_limits<size_t>::max() : 30;

				cov::RunCoverageSettings runCoverageSettings(
				    *startInfo,
				    coverageFilterSettings,
				    options.GetUnifiedDiffSettingsCollection(),
				    options.GetExcludedLineRegexes(),
				    options.GetSubstitutePdbSourcePaths());

				runCoverageSettings.SetCoverChildren(options.IsCoverChildrenModeEnabled());
				runCoverageSettings.SetContinueAfterCppException(options.IsContinueAfterCppExceptionModeEnabled());
                runCoverageSettings.SetStopOnAssert(options.IsStopOnAssertModeEnabled());
                runCoverageSettings.SetMaxUnmatchPathsForWarning(maxUnmatchPathsForWarning);
				runCoverageSettings.SetOptimizedBuildSupport(options.IsOptimizedBuildSupportEnabled());
				runCoverageSettings.SetBranchCoverage(options.IsBranchCoverageModeEnabled());
				runCoverageSettings.SetPdbCacheEnabled(!options.IsNoPdbCacheModeEnabled());
				runCoverageSettings.SetRawPdbEnabled(!options.IsNoRawPdbModeEnabled());
				runCoverageSettings.SetLineMergingEnabled(!options.IsNoLineMergingModeEnabled());
				runCoverageSettings.SetPreparseImports(options.IsPreparseImportsModeEnabled());

				std::optional<Coverage::CoverageData> coverageData;
				if (options.GetEngine() == cov::CoverageEngine::Breakpoint)
				{
					cov::VehCoverageEngine vehEngine{warningManager};
					coverageData.emplace(Tools::ValueOrThrow(
					    vehEngine.RunCoverage(runCoverageSettings)));
				}
				else
				{
					coverageData.emplace(
					    codeCoverageRunner.RunCoverage(runCoverageSettings));
				}
				exitCode = coverageData->GetExitCode();
				coveraDatas.push_back(std::move(*coverageData));
			}

			for (const auto& coveraData : coveraDatas)
				merger.Add(coveraData);

			auto coverageData = merger.ToCoverageData();

			// Aggregation is applied per export type inside Export (Cobertura
			// only); HTML keeps its per-module structure.
			Export(options, coverageData);
			LOG_INFO("The code coverage report is not what you expect? See the FAQ https://github.com/OpenCppCoverage/OpenCppCoverage/wiki/FAQ.");

			if (exitCode)
				LOG_ERROR("Your program stop with error code: {}", exitCode);
			return exitCode;
		}
	}

	//-----------------------------------------------------------------------------
	int OpenCppCoverage::Run(int argc,
	                         const char** argv,
	                         std::wostream* emptyOptionsExplanation) const
	{
		auto warningManager = std::make_shared<Tools::WarningManager>();
		std::vector<std::unique_ptr<cov::IOptionParser>> optionParsers;

		// External export plugins were removed when OpenCppCoverage became a
		// single statically linked executable: their DLL boundary passed STL
		// objects across a heap boundary, which the static CRT does not allow.
		// The built-in Html / Cobertura / Binary exporters remain.
		optionParsers.push_back(std::make_unique<cov::ExportOptionParser>());
		cov::OptionsParser optionsParser{warningManager, std::move(optionParsers)};

		auto options = optionsParser.Parse(argc, argv, emptyOptionsExplanation);
		auto status = FailureExitCode;

		if (options)
		{
			try
			{
				status = ::OpenCppCoverage::Run(*options, warningManager);
			}
			catch (const std::exception& e)
			{
				LOG_ERROR("Error: {}", e.what());
			}
			catch (...)
			{
				LOG_ERROR("Unkown Error");
			}

			warningManager->DisplayWarnings();
			if (options->IsPlugingModeEnabled())
			{
				std::cout << "Press any key to continue... ";
				std::cin.get();
			}
		}

		return status;
	}
}