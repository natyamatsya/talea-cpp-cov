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
#include "ProgramOptions.hpp"

#include <string>

#include "GlazeCli.hpp"
#include "OptionsSpec.hpp"

namespace CppCoverage
{
	//-------------------------------------------------------------------------
	const std::string ProgramOptions::SelectedModulesOption = "modules";
	const std::string ProgramOptions::ExcludedModulesOption = "excluded_modules";
	const std::string ProgramOptions::SelectedSourcesOption = "sources";
	const std::string ProgramOptions::ExcludedSourcesOption = "excluded_sources";
	const std::string ProgramOptions::VerboseOption = "verbose";
	const std::string ProgramOptions::VerboseShortOption = "v";
	const std::string ProgramOptions::QuietOption = "quiet";
	const std::string ProgramOptions::QuietShortOption = "q";
	const std::string ProgramOptions::PluginOption = "plugin";
	const std::string ProgramOptions::HelpOption = "help";
	const std::string ProgramOptions::HelpShortOption = "h";
	const std::string ProgramOptions::ConfigFileOption = "config_file";
	const std::string ProgramOptions::WorkingDirectoryOption = "working_dir";
	const std::string ProgramOptions::NoCoverChildrenOption = "no_cover_children";
	const std::string ProgramOptions::NoAggregateByFileOption =
	    "no_aggregate_by_file";
	const std::string ProgramOptions::ProgramToRunOption = "program_to_run";
	const std::string ProgramOptions::ProgramToRunArgOption = "program_args";
	const std::string ProgramOptions::InputCoverageValue = "input_coverage";
	const std::string ProgramOptions::UnifiedDiffOption = "unified_diff";
	const std::string ProgramOptions::ContinueAfterCppExceptionOption =
	    "continue_after_cpp_exception";
	const std::string ProgramOptions::OptimizedBuildOption = "optimized_build";
	const std::string ProgramOptions::ExcludedLineRegexOption =
	    "excluded_line_regex";
	const std::string ProgramOptions::SubstitutePdbSourcePathOption =
	    "substitute_pdb_source_path";
	const std::string ProgramOptions::StopOnAssertOption = "stop_on_assert";
	const std::string ProgramOptions::BranchCoverageOption = "branch_coverage";
	const std::string ProgramOptions::NoPdbCacheOption = "no_pdb_cache";
	const std::string ProgramOptions::NoRawPdbOption = "no_raw_pdb";
	const std::string ProgramOptions::NoLineMergingOption = "no_line_merging";
	const std::string ProgramOptions::PreparseImportsOption = "preparse_imports";
	const std::string ProgramOptions::EngineOption = "engine";
	const std::string ProgramOptions::EngineInstrumentationValue = "instrumentation";
	const std::string ProgramOptions::EngineBreakpointValue = "breakpoint";
	const std::string ProgramOptions::EngineDebuggerValue = "debugger";
	const std::string ProgramOptions::InputFastCovValue = "input_fastcov";
	const std::string ProgramOptions::FastCovTemplateReportValue =
	    "fastcov_template_report";

	//-------------------------------------------------------------------------
	std::string ProgramOptions::GetHelpMessage()
	{
		std::string message = "OpenCppCoverage Version: ";
		message += OPENCPPCOVERAGE_VERSION;
		message +=
		    "\n\nUsage: [options] -- program_to_run optional_arguments\n\n"
		    "Options:\n";
		message += glzcli::help<OptionsSpec>();
		return message;
	}
}
