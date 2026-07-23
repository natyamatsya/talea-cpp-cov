// OpenCppCoverage is an open source code coverage for C++.
// Copyright (C) 2014 OpenCppCoverage

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

#pragma once

#include <string>

#include "CppCoverageExport.hpp"

namespace CppCoverage
{
	// The canonical option (long-flag) names. These match the field names in
	// OptionsSpec's glz::meta exactly, so they are the single naming authority
	// shared by the parser, the help text and the tests.
	class CPPCOVERAGE_DLL ProgramOptions
	{
	public:
		static const std::string SelectedModulesOption;
		static const std::string ExcludedModulesOption;
		static const std::string SelectedSourcesOption;
		static const std::string ExcludedSourcesOption;
		static const std::string VerboseOption;
		static const std::string QuietOption;
		static const std::string PluginOption;
		static const std::string VerboseShortOption;
		static const std::string QuietShortOption;
		static const std::string HelpOption;
		static const std::string HelpShortOption;
		static const std::string ConfigFileOption;
		static const std::string WorkingDirectoryOption;
		static const std::string NoCoverChildrenOption;
		static const std::string NoAggregateByFileOption;
        static const std::string StopOnAssertOption;
        static const std::string ProgramToRunOption;
		static const std::string ProgramToRunArgOption;
		static const std::string InputCoverageValue;
		static const std::string UnifiedDiffOption;
		static const std::string ContinueAfterCppExceptionOption;
		static const std::string OptimizedBuildOption;
		static const std::string ExcludedLineRegexOption;
		static const std::string SubstitutePdbSourcePathOption;
		static const std::string BranchCoverageOption;
		static const std::string NoPdbCacheOption;
		static const std::string NoRawPdbOption;
		static const std::string NoLineMergingOption;
		static const std::string PreparseImportsOption;
		static const std::string EngineOption;
		static const std::string EngineInstrumentationValue;
		static const std::string EngineBreakpointValue;
		static const std::string EngineDebuggerValue;
		static const std::string InputFastCovValue;
		static const std::string FastCovTemplateReportValue;

		// The version header + reflected option help, printed for --help and on
		// a parse error (replaces the old boost operator<<).
		static std::string GetHelpMessage();
	};
}