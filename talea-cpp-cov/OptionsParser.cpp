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
#include "OptionsParser.hpp"

#include <format>
#include <fstream>
#include <span>
#include <sstream>
#include <string>
#include <vector>
#include <filesystem>

#include "tools/StringUtils.hpp"
#include "tools/Tool.hpp"
#include "talea-cpp-cov/Patterns.hpp"

#include "Options.hpp"
#include "GlazeCli.hpp"
#include "OptionsSpec.hpp"
#include "ProgramOptions.hpp"

#include "tools/WarningManager.hpp"
#include "tools/Log.hpp"
#include "IOptionParser.hpp"

namespace cov = TaleaCppCov;
namespace fs = std::filesystem;

namespace TaleaCppCov
{
	namespace
	{
		//---------------------------------------------------------------------
		// Wraps a user-facing option message (no file:line prefix, unlike
		// MAKE_ERROR) as the Tools::Error carried by the parser's std::expected.
		Tools::Error OptionError(const std::string& message)
		{
			return Tools::Error{Tools::LocalToWString(message)};
		}

		//---------------------------------------------------------------------
		Tools::Expected<void> CheckPattern(const std::string& name,
		                                   const std::string& value)
		{
			auto notWindowsPathSeparator = '/';

			if (value.find(notWindowsPathSeparator) != std::string::npos)
			{
				return std::unexpected(OptionError(
				    "Error: Invalid value \""
				    "--" +
				    name + ' ' + value + "\". " +
				    "Please use Windows path separator '\\'."));
			}

			// Do not iterate over path: path{"Folder\\"}.filename() == '.'
			std::vector<std::string> parts;
			Tools::Split(parts, std::string_view{value}, [](char c) {
				return c == '\\';
			});

			for (const auto& part : parts)
			{
				if (part == "." || part == "..")
				{
					return std::unexpected(OptionError(
					    "Error: \""
					    "--" +
					    name + ' ' + value +
					    "\": Cannot contain \".\" (current folder) or \"..\" "
					    "(parent folder)."));
				}
			}
			return {};
		}

		//---------------------------------------------------------------------
		// The selected patterns default to "*" when none is given.
		Tools::Expected<cov::Patterns>
		GetPatterns(const std::string& selectedName,
		            std::vector<std::string> selected,
		            const std::string& excludedName,
		            const std::vector<std::string>& excluded)
		{
			cov::Patterns patterns{false};

			if (selected.empty())
				selected.push_back("*");

			for (const auto& pattern : selected)
			{
				if (auto checked = CheckPattern(selectedName, pattern); !checked)
					return std::unexpected(std::move(checked.error()));
				patterns.AddSelectedPatterns(Tools::LocalToWString(pattern));
			}
			for (const auto& pattern : excluded)
			{
				if (auto checked = CheckPattern(excludedName, pattern); !checked)
					return std::unexpected(std::move(checked.error()));
				patterns.AddExcludedPatterns(Tools::LocalToWString(pattern));
			}
			return patterns;
		}

		//---------------------------------------------------------------------
		std::optional<cov::StartInfo> GetStartInfo(const OptionsSpec& spec)
		{
			if (spec.program_to_run.empty())
				return std::nullopt;

			cov::StartInfo startInfo{spec.program_to_run};

			for (const auto& arg : spec.program_args)
				startInfo.AddArgument(Tools::LocalToWString(arg));

			if (spec.working_dir)
				startInfo.SetWorkingDirectory(*spec.working_dir);
			return startInfo;
		}

		//---------------------------------------------------------------------
		Tools::Expected<void> AddInputCoverages(const OptionsSpec& spec,
		                                        Options& options)
		{
			for (const auto& path : spec.input_coverage)
			{
				if (!Tools::FileExists(path))
				{
					return std::unexpected(OptionError(
					    "Argument of " + ProgramOptions::InputCoverageValue +
					    " <" + path + "> does not exist."));
				}
				options.AddInputCoveragePath(path);
			}
			return {};
		}

		//---------------------------------------------------------------------
		Tools::Expected<void> AddInputFastCov(const OptionsSpec& spec,
		                                      Options& options)
		{
			for (const auto& path : spec.input_fastcov)
			{
				std::error_code ec;
				if (fs::is_directory(path, ec))
				{
					// A directory argument is expanded recursively to every
					// *.fastcov dump beneath it - the whole output of a parallel
					// test suite, without spelling out (or blowing the command
					// line on) thousands of paths.
					std::uint32_t added = 0;
					for (const auto& entry :
					     fs::recursive_directory_iterator(path, ec))
					{
						if (entry.is_regular_file(ec) &&
						    entry.path().extension() == L".fastcov")
						{
							options.AddInputFastCovPath(entry.path());
							++added;
						}
					}
					if (added == 0)
					{
						return std::unexpected(OptionError(
						    "No .fastcov files found under directory of " +
						    ProgramOptions::InputFastCovValue + " <" + path +
						    ">."));
					}
				}
				else if (Tools::FileExists(path))
				{
					options.AddInputFastCovPath(path);
				}
				else
				{
					return std::unexpected(OptionError(
					    "Argument of " + ProgramOptions::InputFastCovValue +
					    " <" + path + "> does not exist."));
				}
			}

			if (spec.fastcov_template_report)
				options.SetFastCovTemplateReportPath(
				    *spec.fastcov_template_report);
			return {};
		}

		//----------------------------------------------------------------------------
		// Resolves --engine into the Options engine and checks it against the
		// other inputs. Importing a dump selects the instrumentation engine;
		// running a program selects the breakpoint engine unless --engine says
		// otherwise. The three engines are mutually exclusive with each other's
		// input: instrumentation imports a dump and does not run a program, and
		// the breakpoint/debugger engines run a program and do not import a dump.
		Tools::Expected<void> ResolveEngine(const OptionsSpec& spec,
		                                    Options& options)
		{
			const bool hasFastCov = !options.GetInputFastCovPaths().empty();
			const bool hasProgram = options.GetStartInfo() != nullptr;

			CoverageEngine engine;
			if (spec.engine)
			{
				const auto& value = *spec.engine;
				if (value == ProgramOptions::EngineInstrumentationValue)
					engine = CoverageEngine::Instrumentation;
				else if (value == ProgramOptions::EngineBreakpointValue)
					engine = CoverageEngine::Breakpoint;
				else if (value == ProgramOptions::EngineDebuggerValue)
					engine = CoverageEngine::Debugger;
				else
					return std::unexpected(OptionError(
					    "Invalid --" + ProgramOptions::EngineOption + " <" +
					    value + ">. Expected " +
					    ProgramOptions::EngineInstrumentationValue + ", " +
					    ProgramOptions::EngineBreakpointValue + " or " +
					    ProgramOptions::EngineDebuggerValue + "."));
			}
			else
			{
				engine = hasFastCov ? CoverageEngine::Instrumentation
				                    : CoverageEngine::Breakpoint;
			}

			if (engine == CoverageEngine::Instrumentation)
			{
				if (!hasFastCov)
					return std::unexpected(OptionError(
					    "The instrumentation engine imports a dump; pass --" +
					    ProgramOptions::InputFastCovValue + "."));
				if (hasProgram)
					return std::unexpected(OptionError(
					    "The instrumentation engine imports a dump; it does not "
					    "run a program. Drop the program, or select --" +
					    ProgramOptions::EngineOption + " " +
					    ProgramOptions::EngineBreakpointValue + "."));
			}
			else if (hasFastCov)
			{
				return std::unexpected(OptionError(
				    "--" + ProgramOptions::InputFastCovValue +
				    " selects the instrumentation engine and cannot be combined "
				    "with the " +
				    (engine == CoverageEngine::Breakpoint
				         ? ProgramOptions::EngineBreakpointValue
				         : ProgramOptions::EngineDebuggerValue) +
				    " engine."));
			}

			options.SetEngine(engine);
			return {};
		}

		//----------------------------------------------------------------------------
		std::pair<fs::path, std::optional<fs::path>>
		ExtractUnifiedDiffOption(const std::string& option)
		{
			auto pos = option.find(OptionsParser::PathSeparator);

			if (pos == std::string::npos)
				return {option, std::nullopt};

			return {option.substr(0, pos), fs::path{option.substr(pos + 1)}};
		}

		//----------------------------------------------------------------------------
		Tools::Expected<void> AddUnifiedDiff(const OptionsSpec& spec,
		                                     Options& options)
		{
			for (const auto& unifiedDiff : spec.unified_diff)
			{
				fs::path unifiedDiffPath;
				std::optional<fs::path> rootDiffFolder;

				std::tie(unifiedDiffPath, rootDiffFolder) =
				    ExtractUnifiedDiffOption(unifiedDiff);

				if (!fs::is_regular_file(unifiedDiffPath))
				{
					return std::unexpected(OptionError(
					    "Unified diff path " + unifiedDiffPath.string() +
					    " does not exist."));
				}
				if (rootDiffFolder && !is_directory(*rootDiffFolder))
				{
					return std::unexpected(OptionError(
					    "Unified diff root folder " + rootDiffFolder->string() +
					    " does not exist."));
				}

				options.AddUnifiedDiffSettings(
				    UnifiedDiffSettings{unifiedDiffPath, rootDiffFolder});
			}
			return {};
		}

		//----------------------------------------------------------------------------
		void AddExcludedLineRegexes(const OptionsSpec& spec, Options& options)
		{
			for (const auto& excludedLineRegex : spec.excluded_line_regex)
				options.AddExcludedLineRegex(
				    Tools::LocalToWString(excludedLineRegex));
		}

		//---------------------------------------------------------------------
		Tools::Expected<SubstitutePdbSourcePath>
		CreateSubstitutePdbSourcePath(const std::string& paths)
		{
			auto pos = paths.find(OptionsParser::PathSeparator);
			const auto error = "Invalid value for " +
			                   ProgramOptions::SubstitutePdbSourcePathOption +
			                   ". ";
			if (pos == std::string::npos)
			{
				return std::unexpected(OptionError(error + "Cannot find " +
				                                   OptionsParser::PathSeparator +
				                                   '.'));
			}

			auto pdbPath = paths.substr(0, pos);
			if (pdbPath.find('/') != std::string::npos)
			{
				return std::unexpected(OptionError(
				    error + "Path \"" + pdbPath +
				    "\" contains '/' which is not the Windows "
				    "path separator. "
				    "Please use '\\' instead."));
			}
			auto localPath = paths.substr(pos + 1);
			if (!Tools::FileExists(localPath))
			{
				return std::unexpected(OptionError(error + "Path \"" +
				                                   localPath +
				                                   "\" does not exist."));
			}

			return SubstitutePdbSourcePath{pdbPath, localPath};
		}

		//---------------------------------------------------------------------
		Tools::Expected<void> AddSubstitutePdbSourcePaths(const OptionsSpec& spec,
		                                                  Options& options)
		{
			for (const auto& paths : spec.substitute_pdb_source_path)
			{
				auto substitute = CreateSubstitutePdbSourcePath(paths);
				if (!substitute)
					return std::unexpected(std::move(substitute.error()));
				options.AddSubstitutePdbSourcePath(std::move(*substitute));
			}
			return {};
		}

		//---------------------------------------------------------------------------
		void CheckArgumentsSize(int argc,
		                        const char** argv,
		                        Tools::WarningManager& warningManager)
		{
			size_t estimatedSize = 0;
			for (std::int32_t i = 0; i < argc; ++i)
			{
				auto argument = argv[i];
				if (argument)
					estimatedSize += strlen(argument);
			}

			estimatedSize += argc - 1; // separators for arguments.
			if (estimatedSize >=
			    static_cast<size_t>(OptionsParser::DosCommandLineMaxSize * 3 /
			                        4))
			{
				auto tooLongCmd = OptionsParser::GetTooLongCommandLineMessage();
				LOG_WARNING("{}", tooLongCmd);
				warningManager.AddWarning(tooLongCmd);
			}
		}

		//---------------------------------------------------------------------
		// Reads a JSON config file into spec (the base; the command line is
		// applied on top afterwards).
		Tools::Expected<void> ReadConfigFile(OptionsSpec& spec,
		                                     const std::string& path)
		{
			std::ifstream ifs{path, std::ios::binary};
			if (!ifs)
				return std::unexpected(
				    OptionError("Cannot open config file: " + path));

			std::string content{std::istreambuf_iterator<char>{ifs},
			                    std::istreambuf_iterator<char>{}};
			auto error = glz::read_json(spec, content);
			if (error)
				return std::unexpected(OptionError(
				    "Cannot parse JSON config file " + path + ": " +
				    glz::format_error(error, content)));
			return {};
		}

		//---------------------------------------------------------------------
		// Splits the raw arguments (argv[0] skipped) into command-line option
		// tokens and, after the first "--", the program-to-run tokens.
		struct SplitArgs
		{
			std::vector<std::string> options;
			std::vector<std::string> program; // program then its arguments
			bool hasSeparator = false;
		};

		SplitArgs SplitArguments(int argc, const char** argv)
		{
			SplitArgs split;
			for (std::int32_t i = 1; i < argc; ++i)
			{
				std::string token = argv[i] ? argv[i] : "";
				if (!split.hasSeparator && token == "--")
				{
					split.hasSeparator = true;
					continue;
				}
				if (split.hasSeparator)
					split.program.push_back(std::move(token));
				else
					split.options.push_back(std::move(token));
			}
			return split;
		}
	}

	//-------------------------------------------------------------------------
	const char OptionsParser::PathSeparator = '?';
	const std::int32_t OptionsParser::DosCommandLineMaxSize = 8191;

	//-------------------------------------------------------------------------
	OptionsParser::OptionsParser(
	    std::shared_ptr<Tools::WarningManager> warningManager,
	    std::vector<std::unique_ptr<IOptionParser>>&& optionParsers)
	    : optionalWarningManager_{std::move(warningManager)}
	{
		optionParsers_ = std::move(optionParsers);
	}

	//-------------------------------------------------------------------------
	OptionsParser::OptionsParser() : OptionsParser({}, {})
	{
	}

	//-------------------------------------------------------------------------
	OptionsParser::~OptionsParser()
	{
	}

	//-------------------------------------------------------------------------
	std::optional<Options>
	OptionsParser::Parse(int argc,
	                     const char** argv,
	                     std::wostream* emptyOptionsExplanation) const
	{
		auto result = Parse(argc, argv);
		if (!result)
		{
			ShowExplanation(emptyOptionsExplanation,
			                Tools::ToLocalString(result.error().message).c_str());
			return std::nullopt;
		}

		auto& options = *result;
		if (!options && emptyOptionsExplanation)
			*emptyOptionsExplanation
			    << Tools::LocalToWString(ProgramOptions::GetHelpMessage());

		return std::move(options);
	}

	//-------------------------------------------------------------------------
	void OptionsParser::ShowExplanation(std::wostream* emptyOptionsExplanation,
	                                    const char* message) const
	{
		if (emptyOptionsExplanation)
		{
			*emptyOptionsExplanation << message << std::endl;
			*emptyOptionsExplanation
			    << Tools::LocalToWString(ProgramOptions::GetHelpMessage())
			    << std::endl;
		}
	}

	//-------------------------------------------------------------------------
	Tools::Expected<std::optional<Options>>
	OptionsParser::Parse(int argc, const char** argv) const
	{
		if (optionalWarningManager_)
			CheckArgumentsSize(argc, argv, *optionalWarningManager_);

		auto split = SplitArguments(argc, argv);
		std::span<const std::string> optionTokens{split.options};

		// Discover --config_file first (a throwaway parse), so the JSON config
		// can be read as the base with the command line applied on top.
		OptionsSpec probe;
		glzcli::parse(probe, optionTokens);

		OptionsSpec spec;
		if (probe.config_file)
		{
			if (auto configRead = ReadConfigFile(spec, *probe.config_file);
			    !configRead)
				return std::unexpected(std::move(configRead.error()));
		}

		auto result = glzcli::parse(spec, optionTokens);
		if (result.help)
			return std::optional<Options>{std::nullopt};
		if (result.error)
			return std::unexpected(OptionError(*result.error));

		// A "--" separator puts the program and its arguments after it,
		// verbatim; otherwise they were captured as positionals.
		if (split.hasSeparator && !split.program.empty())
		{
			spec.program_to_run = split.program.front();
			spec.program_args.assign(split.program.begin() + 1,
			                         split.program.end());
		}

		auto modulePatterns = GetPatterns(ProgramOptions::SelectedModulesOption,
		                                  spec.modules,
		                                  ProgramOptions::ExcludedModulesOption,
		                                  spec.excluded_modules);
		if (!modulePatterns)
			return std::unexpected(std::move(modulePatterns.error()));
		auto sourcePatterns = GetPatterns(ProgramOptions::SelectedSourcesOption,
		                                  spec.sources,
		                                  ProgramOptions::ExcludedSourcesOption,
		                                  spec.excluded_sources);
		if (!sourcePatterns)
			return std::unexpected(std::move(sourcePatterns.error()));

		auto optionalStartInfo = GetStartInfo(spec);
		Options options{*modulePatterns,
		                *sourcePatterns,
		                optionalStartInfo ? &*optionalStartInfo : nullptr};

		if (spec.verbose && spec.quiet)
			return std::unexpected(OptionError(
			    "--" + ProgramOptions::VerboseOption + " and --" +
			    ProgramOptions::QuietOption +
			    " cannot be used at the same time."));

		if (spec.verbose)
			options.SetLogLevel(LogLevel::Verbose);
		if (spec.quiet)
			options.SetLogLevel(LogLevel::Quiet);

		if (spec.no_cover_children)
			options.DisableCoverChildrenMode();
		if (spec.plugin)
			options.EnablePlugingMode();
		if (spec.no_aggregate_by_file)
			options.DisableAggregateByFileMode();
		if (spec.continue_after_cpp_exception)
			options.EnableContinueAfterCppExceptionMode();
		if (spec.optimized_build)
			options.EnableOptimizedBuildSupport();
		if (spec.stop_on_assert)
			options.EnableStopOnAssertMode();
		if (spec.branch_coverage)
			options.EnableBranchCoverageMode();
		if (spec.no_pdb_cache)
			options.EnableNoPdbCacheMode();
		if (spec.no_raw_pdb)
			options.EnableNoRawPdbMode();
		if (spec.no_line_merging)
			options.EnableNoLineMergingMode();
		if (spec.preparse_imports)
			options.EnablePreparseImportsMode();

		if (auto added = AddInputCoverages(spec, options); !added)
			return std::unexpected(std::move(added.error()));
		if (auto added = AddInputFastCov(spec, options); !added)
			return std::unexpected(std::move(added.error()));
		if (auto added = AddUnifiedDiff(spec, options); !added)
			return std::unexpected(std::move(added.error()));
		AddExcludedLineRegexes(spec, options);
		if (auto added = AddSubstitutePdbSourcePaths(spec, options); !added)
			return std::unexpected(std::move(added.error()));

		if (!options.GetStartInfo() && options.GetInputCoveragePaths().empty() &&
		    options.GetInputFastCovPaths().empty())
			return std::unexpected(OptionError(
			    "You must specify a program to execute or use --" +
			    ProgramOptions::InputCoverageValue + " or --" +
			    ProgramOptions::InputFastCovValue));

		if (auto resolved = ResolveEngine(spec, options); !resolved)
			return std::unexpected(std::move(resolved.error()));

		for (const auto& optionParser : optionParsers_)
		{
			if (auto parsed = optionParser->ParseOption(spec, options); !parsed)
				return std::unexpected(std::move(parsed.error()));
		}
		return std::optional<Options>{std::move(options)};
	}

	//-------------------------------------------------------------------------
	std::wstring OptionsParser::GetTooLongCommandLineMessage()
	{
		return std::format(
		    L"You have a very long command line. It might be truncated as DOS "
		    L"maximum command line size is {}. Please consider using --{} "
		    L"instead.",
		    DosCommandLineMaxSize,
		    Tools::LocalToWString(ProgramOptions::ConfigFileOption));
	}
}
