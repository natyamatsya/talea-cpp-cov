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

#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <glaze/glaze.hpp>

namespace CppCoverage
{
	// The single source of truth for command-line options, parsed by GlazeCli
	// via glaze reflection and (for --config_file) loaded from JSON with the
	// same struct. Field types encode CLI shape: bool = presence flag,
	// std::optional<T> = value option, std::vector<std::string> = repeatable +
	// comma-split. The flag names in the glz::meta below match the historical
	// boost::program_options names exactly, so the command line is unchanged.
	//
	// The program to run and its arguments come after "--" and are handled by
	// OptionsParser (not part of this struct). Defaulted vectors (modules,
	// sources, export_type) are left empty here and defaulted after parsing, so
	// user-supplied values replace rather than append to the default.
	struct OptionsSpec
	{
		// --- flags ---
		bool verbose = false;
		bool quiet = false;
		bool plugin = false;
		bool cover_children = false;
		bool no_aggregate_by_file = false;
		bool stop_on_assert = false;
		bool continue_after_cpp_exception = false;
		bool optimized_build = false;
		bool branch_coverage = false;
		bool no_pdb_cache = false;
		bool no_raw_pdb = false;
		bool no_line_merging = false;
		bool preparse_imports = false;
		bool experimental_veh = false; // deprecated: VEH is the default engine
		bool no_veh = false;

		// --- scalar value options ---
		std::optional<std::string> config_file;
		std::optional<std::string> working_dir;
		std::optional<std::string> fastcov_template_report;

		// --- repeatable value options ---
		std::vector<std::string> modules;
		std::vector<std::string> excluded_modules;
		std::vector<std::string> sources;
		std::vector<std::string> excluded_sources;
		std::vector<std::string> input_coverage;
		std::vector<std::string> unified_diff;
		std::vector<std::string> input_fastcov;
		std::vector<std::string> excluded_line_regex;
		std::vector<std::string> substitute_pdb_source_path;
		std::vector<std::string> export_type;

		// --- positionals: the program to run and its arguments ---
		// Filled from bare tokens (no "--") or from the tokens after a "--"
		// separator (handled by OptionsParser). Not shown in help.
		std::string program_to_run;
		std::vector<std::string> program_args;

		static constexpr std::array shorts = {
		    std::pair{'v', std::string_view{"verbose"}},
		    std::pair{'q', std::string_view{"quiet"}}};
		static constexpr std::array<std::string_view, 2> positionals = {
		    "program_to_run", "program_args"};

		static constexpr std::array descriptions = {
		    std::pair{std::string_view{"verbose"},
		              std::string_view{"Verbose mode."}},
		    std::pair{std::string_view{"quiet"},
		              std::string_view{"Quiet mode."}},
		    std::pair{std::string_view{"config_file"},
		              std::string_view{"Filename of a JSON configuration file."}},
		    std::pair{
		        std::string_view{"modules"},
		        std::string_view{"The pattern that module's paths should match. "
		                         "Can have multiple occurrences."}},
		    std::pair{std::string_view{"excluded_modules"},
		              std::string_view{
		                  "The pattern that module's paths should NOT match. "
		                  "Can have multiple occurrences."}},
		    std::pair{
		        std::string_view{"sources"},
		        std::string_view{"The pattern that source's paths should match. "
		                         "Can have multiple occurrences."}},
		    std::pair{std::string_view{"excluded_sources"},
		              std::string_view{
		                  "The pattern that source's paths should NOT match. "
		                  "Can have multiple occurrences."}},
		    std::pair{std::string_view{"input_coverage"},
		              std::string_view{
		                  "A binary export to merge with the current coverage. "
		                  "Can have multiple occurrences."}},
		    std::pair{std::string_view{"working_dir"},
		              std::string_view{"The program working directory."}},
		    std::pair{std::string_view{"cover_children"},
		              std::string_view{
		                  "Enable code coverage for children processes."}},
		    std::pair{std::string_view{"no_aggregate_by_file"},
		              std::string_view{
		                  "Do not aggregate coverage for same file path."}},
		    std::pair{
		        std::string_view{"stop_on_assert"},
		        std::string_view{"Do not continue after DebugBreak() or assert()."}},
		    std::pair{std::string_view{"unified_diff"},
		              std::string_view{
		                  "Restrict coverage to a unified diff. Format: "
		                  "<unifiedDiffPath>?<rootFolder>."}},
		    std::pair{std::string_view{"continue_after_cpp_exception"},
		              std::string_view{
		                  "Try to continue after throwing a C++ exception."}},
		    std::pair{std::string_view{"optimized_build"},
		              std::string_view{
		                  "Enable heuristics to support optimized build."}},
		    std::pair{std::string_view{"branch_coverage"},
		              std::string_view{
		                  "(Experimental) Also measure branch coverage "
		                  "(conditional jumps)."}},
		    std::pair{std::string_view{"no_pdb_cache"},
		              std::string_view{
		                  "Disable the PDB line-table cache (memory and disk)."}},
		    std::pair{std::string_view{"no_raw_pdb"},
		              std::string_view{
		                  "Parse PDBs with DIA instead of the built-in raw "
		                  "reader."}},
		    std::pair{std::string_view{"no_line_merging"},
		              std::string_view{
		                  "One breakpoint per source line instead of per "
		                  "straight-line run. Coverage is identical."}},
		    std::pair{std::string_view{"preparse_imports"},
		              std::string_view{
		                  "Parse the executable and its imported DLLs' PDBs in "
		                  "parallel before start. Pure performance."}},
		    std::pair{std::string_view{"experimental_veh"},
		              std::string_view{
		                  "Deprecated: the in-process VEH engine is now the "
		                  "default; this flag has no effect. Use --no_veh to "
		                  "disable it."}},
		    std::pair{std::string_view{"no_veh"},
		              std::string_view{
		                  "Disable the in-process VEH engine and use the classic "
		                  "debugger. Slower, but re-instruments unloaded-then-"
		                  "reloaded DLLs, which VEH does not."}},
		    std::pair{std::string_view{"input_fastcov"},
		              std::string_view{
		                  "(Experimental) Import a compiler-instrumentation "
		                  "(SanitizerCoverage) dump. Multiple occurrences."}},
		    std::pair{std::string_view{"fastcov_template_report"},
		              std::string_view{
		                  "Write the per-template instantiation coverage report "
		                  "(from --input_fastcov) as Markdown to this path."}},
		    std::pair{std::string_view{"excluded_line_regex"},
		              std::string_view{
		                  "Exclude all lines matching the regular expression "
		                  "(must match the whole line)."}},
		    std::pair{std::string_view{"substitute_pdb_source_path"},
		              std::string_view{
		                  "Substitute the pdb start path by a local path. "
		                  "Format: <pdbStartPath>?<localPath>."}},
		    std::pair{std::string_view{"export_type"},
		              std::string_view{
		                  "Export type: html, cobertura or binary, each "
		                  "optionally :<output>. Multiple occurrences. Defaults "
		                  "to html when omitted."}}};
	};
}

namespace glz
{
	template <>
	struct meta<CppCoverage::OptionsSpec>
	{
		using T = CppCoverage::OptionsSpec;
		static constexpr auto value = object("verbose",
		                                     &T::verbose,
		                                     "quiet",
		                                     &T::quiet,
		                                     "plugin",
		                                     &T::plugin,
		                                     "cover_children",
		                                     &T::cover_children,
		                                     "no_aggregate_by_file",
		                                     &T::no_aggregate_by_file,
		                                     "stop_on_assert",
		                                     &T::stop_on_assert,
		                                     "continue_after_cpp_exception",
		                                     &T::continue_after_cpp_exception,
		                                     "optimized_build",
		                                     &T::optimized_build,
		                                     "branch_coverage",
		                                     &T::branch_coverage,
		                                     "no_pdb_cache",
		                                     &T::no_pdb_cache,
		                                     "no_raw_pdb",
		                                     &T::no_raw_pdb,
		                                     "no_line_merging",
		                                     &T::no_line_merging,
		                                     "preparse_imports",
		                                     &T::preparse_imports,
		                                     "experimental_veh",
		                                     &T::experimental_veh,
		                                     "no_veh",
		                                     &T::no_veh,
		                                     "config_file",
		                                     &T::config_file,
		                                     "working_dir",
		                                     &T::working_dir,
		                                     "fastcov_template_report",
		                                     &T::fastcov_template_report,
		                                     "modules",
		                                     &T::modules,
		                                     "excluded_modules",
		                                     &T::excluded_modules,
		                                     "sources",
		                                     &T::sources,
		                                     "excluded_sources",
		                                     &T::excluded_sources,
		                                     "input_coverage",
		                                     &T::input_coverage,
		                                     "unified_diff",
		                                     &T::unified_diff,
		                                     "input_fastcov",
		                                     &T::input_fastcov,
		                                     "excluded_line_regex",
		                                     &T::excluded_line_regex,
		                                     "substitute_pdb_source_path",
		                                     &T::substitute_pdb_source_path,
		                                     "export_type",
		                                     &T::export_type,
		                                     "program_to_run",
		                                     &T::program_to_run,
		                                     "program_args",
		                                     &T::program_args);
	};
}
