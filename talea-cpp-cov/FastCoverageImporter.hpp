// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 natyamatsya and talea-cpp-cov contributors

#pragma once
#include <cstdint>
#include <memory>

#include <filesystem>
#include <string>
#include <vector>

#include "TaleaCppCovExport.hpp"
#include "coverage/CoverageData.hpp"
#include "SubstitutePdbSourcePath.hpp"

#include "tools/Error.hpp"

namespace TaleaCppCov
{
	class CoverageFilterSettings;
	class UnifiedDiffSettings;

	// Imports a "fast mode" coverage dump (produced by the SanitizerCoverage
	// runtime the target links) and turns it into coverage data by mapping each
	// instrumented basic block back to source lines through the PDB.
	//
	// Because the compiler emits a distinct function per template instantiation,
	// the importer also recovers a per-template, per-instantiation view: every
	// basic block is attributed to its containing function (the instantiation),
	// so a line covered for foo<std::int32_t> but not foo<double> is visible instead of
	// collapsed into a single boolean.
	class TALEACPPCOV_DLL FastCoverageImporter
	{
	  public:
		// A source line of one instantiation, with its execution count (a
		// saturating 8-bit counter from the runtime).
		struct InstantiationLine
		{
			std::uint32_t lineNumber_;
			std::uint32_t count_;
		};

		// One template instantiation (e.g. "foo<std::int32_t>"), its definition file,
		// and its per-line counts.
		struct Instantiation
		{
			std::wstring name_;
			std::filesystem::path file_;
			std::vector<InstantiationLine> lines_;
		};

		// A template (e.g. "foo") and every instantiation the program contains.
		struct TemplateReport
		{
			std::wstring stem_;
			std::vector<Instantiation> instantiations_;
		};

		struct Result
		{
			Coverage::CoverageData coverageData_;
			std::vector<TemplateReport> templates_;
		};

		FastCoverageImporter(
		    const CoverageFilterSettings&,
		    const std::vector<UnifiedDiffSettings>&,
		    const std::vector<std::wstring>& excludedLineRegexes,
		    const std::vector<SubstitutePdbSourcePath>&);
		~FastCoverageImporter();

		[[nodiscard]] Tools::Expected<Result> Import(const std::filesystem::path& dumpPath);

		// Renders the per-template report as GitHub-flavored Markdown.
		static std::wstring
		FormatTemplateReport(const std::vector<TemplateReport>&);

	  private:
		// The dump parser: format errors come back by value; the per-module
		// work can still throw and is caught by Import().
		Tools::Expected<Result> ImportOrThrow(const std::filesystem::path& dumpPath);

		FastCoverageImporter(const FastCoverageImporter&) = delete;
		FastCoverageImporter& operator=(const FastCoverageImporter&) = delete;

		const CoverageFilterSettings& coverageFilterSettings_;
		const std::vector<UnifiedDiffSettings>& unifiedDiffSettings_;
		const std::vector<std::wstring>& excludedLineRegexes_;
		const std::vector<SubstitutePdbSourcePath>& substitutePdbSourcePaths_;

		// Counter-independent per-module reconstruction (PDB records + CFG),
		// memoized across Import() calls so a suite's many dumps of the same
		// modules each cost only a counter-apply, not a re-parse + re-disassemble.
		struct ModuleCache;
		std::unique_ptr<ModuleCache> moduleCache_;
	};
}
