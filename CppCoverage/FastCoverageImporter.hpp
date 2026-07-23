// OpenCppCoverage is an open source code coverage for C++.
// Copyright (C) 2026 OpenCppCoverage
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
#include <cstdint>

#include <filesystem>
#include <string>
#include <vector>

#include "CppCoverageExport.hpp"
#include "Plugin/Exporter/CoverageData.hpp"
#include "SubstitutePdbSourcePath.hpp"

#include "Tools/Error.hpp"

namespace CppCoverage
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
	class CPPCOVERAGE_DLL FastCoverageImporter
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
			Plugin::CoverageData coverageData_;
			std::vector<TemplateReport> templates_;
		};

		FastCoverageImporter(
		    const CoverageFilterSettings&,
		    const std::vector<UnifiedDiffSettings>&,
		    const std::vector<std::wstring>& excludedLineRegexes,
		    const std::vector<SubstitutePdbSourcePath>&);

		Tools::Expected<Result> Import(const std::filesystem::path& dumpPath);

		// Renders the per-template report as GitHub-flavored Markdown.
		static std::wstring
		FormatTemplateReport(const std::vector<TemplateReport>&);

	  private:
		// The dump parser, which reports format errors by throwing
		// CppCoverageException; Import() adapts it to the Expected boundary.
		Result ImportOrThrow(const std::filesystem::path& dumpPath);

		FastCoverageImporter(const FastCoverageImporter&) = delete;
		FastCoverageImporter& operator=(const FastCoverageImporter&) = delete;

		const CoverageFilterSettings& coverageFilterSettings_;
		const std::vector<UnifiedDiffSettings>& unifiedDiffSettings_;
		const std::vector<std::wstring>& excludedLineRegexes_;
		const std::vector<SubstitutePdbSourcePath>& substitutePdbSourcePaths_;
	};
}
