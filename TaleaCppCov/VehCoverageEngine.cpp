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

#include "stdafx.h"
#include <cstdint>
#include "VehCoverageEngine.hpp"

#include <Windows.h>

#include <algorithm>
#include <format>
#include <iterator>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <system_error>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "Tools/Log.hpp"
#include "Tools/Tool.hpp"
#include "Tools/WarningManager.hpp"

#include "Address.hpp"
#include "BranchDisassembler.hpp"
#include "BranchInstruction.hpp"
#include "TaleaCppCovException.hpp"
#include "Tools/Error.hpp"
#include "CoverageFilterManager.hpp"
#include "DebugInformationEnumerator.hpp"
#include "FileSystem.hpp"
#include "FilterAssistant.hpp"
#include "ExecutedAddressManager.hpp"
#include "LineRunScanner.hpp"
#include "PdbLineTableCache.hpp"
#include "PeImageFileReader.hpp"
#include "RunCoverageSettings.hpp"
#include "StartInfo.hpp"

#include "FileFilter/ModuleInfo.hpp"
#include "FileFilter/FileInfo.hpp"
#include "FileFilter/LineInfo.hpp"

#include "Coverage/CoverageData.hpp"
#include "Coverage/ModuleCoverage.hpp"
#include "Coverage/FileCoverage.hpp"
#include "Coverage/LineCoverage.hpp"

#include "VehCoverageHelper/VehProtocol.hpp"
#include "VehCoverageHelper/VehBranchEval.hpp"

namespace TaleaCppCov
{
	namespace
	{
		constexpr DWORD HelperReadyTimeoutMs = 60000;
		// Bounds on the exe-directory scan, so a pathological tree cannot make
		// startup unbounded.
		constexpr std::size_t MaxDiscoveredModules = 256;
		constexpr std::size_t MaxScannedFiles = 50000;

		//---------------------------------------------------------------------
		// A source line in the module's RVA space. The file is an index into
		// the module's interned file table (ModuleProbes::files_): a per-line
		// path COPY made a statically-linked-heavy test exe cost ~100 bytes
		// of path per line, and a 50-exe suite ran the host into the tens of
		// gigabytes.
		struct SelectedLine
		{
			std::uint32_t rva_;
			std::uint32_t length_;
			std::uint32_t lineNumber_;
			std::uint32_t fileIndex_;
		};

		// One instrumented module: the probe leader RVAs the helper patches,
		// and, aligned with them, the (file, line)s each leader covers, so the
		// coverage data can be built from the hit bitmap after the run.
		struct ModuleBranch
		{
			std::uint32_t rva_;
			std::uint8_t originalByte_;
			VehCoverage::BranchCondition condition_;
			std::uint32_t fileIndex_;
			std::uint32_t lineNumber_;
			std::uint32_t branchIndex_;
		};

		struct ModuleProbes
		{
			std::filesystem::path path_;
			std::wstring baseNameLower_;
			std::vector<std::wstring> files_; // interned source-file paths
			std::vector<std::uint32_t> probeRvas_;
			// (fileIndex, lineNumber) pairs per probe, indices into files_.
			std::vector<std::vector<std::pair<std::uint32_t, std::uint32_t>>>
			    probeLines_;
			std::vector<ModuleBranch> branches_; // sorted by rva_, may be empty
		};

		//---------------------------------------------------------------------
		// Maps the debugger engine's branch-condition enum onto the shared one
		// the helper evaluates. An explicit switch, so a reordering of either
		// enum becomes a compile error rather than a silent mismatch.
		VehCoverage::BranchCondition ToVehCondition(BranchCondition condition)
		{
			using Src = BranchCondition;
			using Dst = VehCoverage::BranchCondition;
			switch (condition)
			{
				case Src::Overflow: return Dst::Overflow;
				case Src::NoOverflow: return Dst::NoOverflow;
				case Src::Below: return Dst::Below;
				case Src::AboveOrEqual: return Dst::AboveOrEqual;
				case Src::Zero: return Dst::Zero;
				case Src::NotZero: return Dst::NotZero;
				case Src::BelowOrEqual: return Dst::BelowOrEqual;
				case Src::Above: return Dst::Above;
				case Src::Sign: return Dst::Sign;
				case Src::NoSign: return Dst::NoSign;
				case Src::Parity: return Dst::Parity;
				case Src::NoParity: return Dst::NoParity;
				case Src::Less: return Dst::Less;
				case Src::GreaterOrEqual: return Dst::GreaterOrEqual;
				case Src::LessOrEqual: return Dst::LessOrEqual;
				case Src::Greater: return Dst::Greater;
				case Src::CxZero: return Dst::CxZero;
				case Src::EcxZero: return Dst::EcxZero;
				case Src::RcxZero: return Dst::RcxZero;
			}
			std::unreachable();
		}

		//---------------------------------------------------------------------
		std::wstring ToLowerAscii(const std::wstring& value)
		{
			std::wstring result;
			result.reserve(value.size());
			for (auto c : value)
				result.push_back(VehCoverage::AsciiToLower(c));
			return result;
		}

		//---------------------------------------------------------------------
		// Collects the function ranges and the filtered, selected lines of a
		// module straight from its PDB - no live process needed. Mirrors what
		// MonitoredLineRegister does in the debugger engine, minus the process.
		class ProbeCollector : public IDebugInformationHandler
		{
		  public:
			ProbeCollector(ICoverageFilterManager& filterManager,
			               const std::filesystem::path& modulePath,
			               FilterAssistant* filterAssistant)
			    : filterManager_{filterManager}, modulePath_{modulePath},
			      filterAssistant_{filterAssistant}
			{
			}

			bool IsSourceFileSelected(
			    const std::filesystem::path& path) override
			{
				auto isSelected =
				    filterManager_.IsSourceFileSelected(path.wstring());
				// Feed the filter assistant like the debugger's
				// MonitoredLineRegister does, so a run that selects modules but
				// no sources still advises a --sources filter.
				if (filterAssistant_)
					filterAssistant_->OnNewSourceFile(path, isSelected);
				return isSelected;
			}

			void OnModuleFunctions(
			    const std::vector<FunctionRange>& functions) override
			{
				functions_ = functions;
			}

			void OnSourceFile(const std::filesystem::path& path,
			                  const std::vector<Line>& lines) override
			{
				std::vector<FileFilter::LineInfo> lineInfos;
				lineInfos.reserve(lines.size());
				for (const auto& line : lines)
				{
					lineInfos.emplace_back(line.lineNumber_,
					                       line.virtualAddress_,
					                       line.symbolIndex_);
				}

				FileFilter::FileInfo fileInfo{path, std::move(lineInfos)};
				// baseOfImage / hProcess are only read by the optimized-build
				// filter, which the VEH engine never enables.
				FileFilter::ModuleInfo moduleInfo{nullptr, modulePath_,
				                                  nullptr};

				// Intern the file once; every selected line refers to it by
				// index (see SelectedLine::fileIndex_).
				auto fileIndex = static_cast<std::uint32_t>(files_.size());
				files_.push_back(path.wstring());
				bool fileUsed = false;

				const auto& collection = fileInfo.lineInfoColllection_;
				for (std::size_t i = 0; i < collection.size(); ++i)
				{
					const auto& lineInfo = collection[i];
					if (!filterManager_.IsLineSelected(
					        moduleInfo, fileInfo, lineInfo))
						continue;

					fileUsed = true;
					selectedLines_.push_back(SelectedLine{
					    static_cast<std::uint32_t>(lineInfo.virtualAddress_),
					    lines[i].length_,
					    static_cast<std::uint32_t>(lineInfo.lineNumber_),
					    fileIndex});
				}
				if (!fileUsed)
					files_.pop_back();
			}

			const std::vector<FunctionRange>& GetFunctions() const
			{
				return functions_;
			}
			std::vector<SelectedLine>& GetSelectedLines()
			{
				return selectedLines_;
			}
			std::vector<std::wstring>& GetFiles()
			{
				return files_;
			}

		  private:
			ICoverageFilterManager& filterManager_;
			const std::filesystem::path modulePath_;
			FilterAssistant* filterAssistant_;
			std::vector<FunctionRange> functions_;
			std::vector<SelectedLine> selectedLines_;
			std::vector<std::wstring> files_;
		};

		//---------------------------------------------------------------------
		// Reduces the selected lines to probe leaders (one per straight-line
		// run when merging applies). Fills probeRvas (sorted, unique) and, for
		// each leader, the (file, line)s it covers. One leader can carry many
		// lines (run merging); one line can appear under several leaders (a
		// line split across runs).
		void BuildProbes(const std::filesystem::path& modulePath,
		                 bool enableLineMerging,
		                 ProbeCollector& collector,
		                 ModuleProbes& out)
		{
			auto& lines = collector.GetSelectedLines();
			std::sort(lines.begin(),
			          lines.end(),
			          [](const auto& lhs, const auto& rhs) {
				          return lhs.rva_ < rhs.rva_;
			          });

			// leaderRva -> selected-line indices registered under it.
			std::unordered_map<std::uint32_t, std::vector<std::size_t>>
			    linesByLeader;
			std::vector<std::uint32_t> leaderOrder;
			auto addProbe = [&](std::uint32_t leaderRva, std::size_t index) {
				auto [it, inserted] = linesByLeader.emplace(
				    leaderRva, std::vector<std::size_t>{});
				if (inserted)
					leaderOrder.push_back(leaderRva);
				it->second.push_back(index);
			};

			if (!enableLineMerging)
			{
				for (std::size_t i = 0; i < lines.size(); ++i)
					addProbe(lines[i].rva_, i);
			}
			else
			{
				auto reader = PeImageFileReader::TryCreate(modulePath);
				LineRunScanner scanner{/*is64BitModule*/ true};

				constexpr std::uint32_t MaxScannedFunctionSize = 1 << 20;
				std::size_t cursor = 0;
				for (const auto& function : collector.GetFunctions())
				{
					auto functionStart =
					    static_cast<std::uint32_t>(function.virtualAddress_);
					auto functionEnd = functionStart + function.length_;

					while (cursor < lines.size() &&
					       lines[cursor].rva_ < functionStart)
					{
						addProbe(lines[cursor].rva_, cursor);
						++cursor;
					}

					auto firstLine = cursor;
					while (cursor < lines.size() &&
					       lines[cursor].rva_ < functionEnd)
						++cursor;
					if (firstLine == cursor)
						continue;

					std::optional<std::vector<LineRun>> runs;
					if (reader && function.length_ <= MaxScannedFunctionSize)
					{
						auto buffer = reader->ReadCode(functionStart,
						                               function.length_);
						if (!buffer.empty())
						{
							std::vector<LineRunLine> scannerLines;
							scannerLines.reserve(cursor - firstLine);
							for (auto i = firstLine; i < cursor; ++i)
								scannerLines.push_back(LineRunLine{
								    lines[i].rva_, lines[i].length_});
							runs = scanner.ComputeRuns(buffer.data(),
							                           buffer.size(),
							                           functionStart,
							                           scannerLines);
						}
					}

					if (runs)
					{
						std::vector<bool> assigned(cursor - firstLine, false);
						for (const auto& run : *runs)
						{
							for (auto lineIndex : run.lineIndices_)
							{
								addProbe(static_cast<std::uint32_t>(
								             run.leaderVirtualAddress_),
								         firstLine + lineIndex);
								assigned[lineIndex] = true;
							}
						}
						for (std::size_t i = 0; i < assigned.size(); ++i)
						{
							if (!assigned[i])
								addProbe(lines[firstLine + i].rva_,
								         firstLine + i);
						}
					}
					else
					{
						for (auto i = firstLine; i < cursor; ++i)
							addProbe(lines[i].rva_, i);
					}
				}
				for (; cursor < lines.size(); ++cursor)
					addProbe(lines[cursor].rva_, cursor);
			}

			std::sort(leaderOrder.begin(), leaderOrder.end());
			out.probeRvas_ = leaderOrder;
			out.probeLines_.resize(leaderOrder.size());
			for (std::size_t i = 0; i < leaderOrder.size(); ++i)
			{
				auto& target = out.probeLines_[i];
				for (auto lineIndex : linesByLeader[leaderOrder[i]])
					target.emplace_back(lines[lineIndex].fileIndex_,
					                    lines[lineIndex].lineNumber_);
			}
		}

		//---------------------------------------------------------------------
		// Disassembles the function enclosing each selected line (from the PE
		// file, no live process) to find its conditional jumps, and records a
		// branch probe per distinct Jcc inside the line's range. Mirrors the
		// debugger's RegisterBranches: same function-scoped, validated Zydis
		// discovery (PDB line ranges can cover inline jump-table data, so a
		// raw per-line sweep must never be patched), same per-line branch
		// index.
		void DiscoverBranches(
		    const std::filesystem::path& modulePath,
		    const std::vector<IDebugInformationHandler::FunctionRange>&
		        functions,
		    const std::vector<SelectedLine>& selectedLines,
		    ModuleProbes& out)
		{
			auto reader = PeImageFileReader::TryCreate(modulePath);
			if (!reader)
				return;

			BranchDisassembler disassembler{/*is64BitCode*/ true};

			// One validated scan per function, cached: the code bytes (for
			// the original byte under each probe) and the branches, or
			// std::nullopt when the function was rejected.
			struct ScannedFunction
			{
				std::uint32_t startRva_;
				std::vector<std::uint8_t> code_;
				std::optional<std::vector<BranchInstruction>> branches_;
			};
			std::unordered_map<std::uint32_t, ScannedFunction>
			    scannedFunctions;
			constexpr std::uint32_t MaxScannedFunctionSize = 1 << 20;

			auto scanEnclosingFunction =
			    [&](std::uint32_t lineRva) -> const ScannedFunction* {
				auto next = std::upper_bound(
				    functions.begin(),
				    functions.end(),
				    lineRva,
				    [](std::uint32_t value,
				       const IDebugInformationHandler::FunctionRange&
				           function) {
					    return static_cast<std::int64_t>(value) <
					           function.virtualAddress_;
				    });
				if (next == functions.begin())
					return nullptr;
				const auto& function = *std::prev(next);
				if (static_cast<std::int64_t>(lineRva) >=
				    function.virtualAddress_ +
				        static_cast<std::int64_t>(function.length_))
					return nullptr;

				auto startRva =
				    static_cast<std::uint32_t>(function.virtualAddress_);
				auto [cached, inserted] = scannedFunctions.emplace(
				    startRva, ScannedFunction{startRva, {}, std::nullopt});
				auto& scanned = cached->second;
				if (!inserted)
					return &scanned;

				if (function.length_ <= MaxScannedFunctionSize)
				{
					scanned.code_ =
					    reader->ReadCode(startRva, function.length_);
					if (!scanned.code_.empty())
						scanned.branches_ =
						    disassembler.FindConditionalBranches(
						        scanned.code_.data(),
						        scanned.code_.size(),
						        startRva);
				}
				return &scanned;
			};

			std::map<std::pair<std::uint32_t, std::uint32_t>, std::uint32_t>
			    branchIndexByLine; // keyed by (fileIndex, lineNumber)
			std::unordered_set<std::uint32_t> seenRvas; // first Jcc wins

			for (const auto& line : selectedLines)
			{
				if (line.length_ == 0)
					continue;
				const auto* scanned = scanEnclosingFunction(line.rva_);
				if (!scanned || !scanned->branches_)
					continue;

				auto lineEnd = line.rva_ + line.length_;
				for (const auto& branch : *scanned->branches_)
				{
					auto rva =
					    static_cast<std::uint32_t>(branch.virtualAddress_);
					if (rva < line.rva_ || rva >= lineEnd)
						continue;
					if (!seenRvas.insert(rva).second)
						continue;

					auto key =
					    std::make_pair(line.fileIndex_, line.lineNumber_);
					auto branchIndex = branchIndexByLine[key]++;
					out.branches_.push_back(
					    ModuleBranch{rva,
					                 scanned->code_[rva - scanned->startRva_],
					                 ToVehCondition(branch.condition_),
					                 line.fileIndex_,
					                 line.lineNumber_,
					                 branchIndex});
				}
			}

			// The helper binary-searches the branch table by RVA.
			std::sort(out.branches_.begin(),
			          out.branches_.end(),
			          [](const ModuleBranch& lhs, const ModuleBranch& rhs) {
				          return lhs.rva_ < rhs.rva_;
			          });
		}

		//---------------------------------------------------------------------
		// Computes the probe spec for one module from its PDB and file, offline.
		// Returns nullopt when the module has no debug information or no
		// selected line to instrument.
		std::optional<ModuleProbes>
		ComputeModuleProbes(const std::filesystem::path& modulePath,
		                    ICoverageFilterManager& filterManager,
		                    const RunCoverageSettings& settings,
		                    std::shared_ptr<PdbLineTableCache> pdbLineTableCache,
		                    FilterAssistant* filterAssistant)
		{
			DebugInformationEnumerator enumerator{
			    settings.GetSubstitutePdbSourcePaths(),
			    /*needSymbolIndex*/ false,
			    pdbLineTableCache,
			    /*needFunctionNames*/ false,
			    settings.IsRawPdbEnabled()};
			ProbeCollector collector{filterManager, modulePath, filterAssistant};
			if (!Tools::ValueOrThrow(enumerator.Enumerate(modulePath, collector)))
				return std::nullopt;

			ModuleProbes probes;
			probes.path_ = modulePath;
			probes.baseNameLower_ =
			    ToLowerAscii(modulePath.filename().wstring());
			BuildProbes(modulePath,
			            settings.IsLineMergingEnabled(),
			            collector,
			            probes);
			probes.files_ = std::move(collector.GetFiles());
			if (probes.probeRvas_.empty())
				return std::nullopt;

			if (settings.GetBranchCoverage())
				DiscoverBranches(modulePath,
				                 collector.GetFunctions(),
				                 collector.GetSelectedLines(),
				                 probes);
			return probes;
		}

		//---------------------------------------------------------------------
		// The main executable plus every selected module with a distinct base
		// name found under its directory. Scanning the directory (rather than
		// walking imports) also catches dynamically loaded modules, as long as
		// they live next to the executable.
		std::vector<std::filesystem::path>
		DiscoverModulePaths(const std::filesystem::path& exePath,
		                    ICoverageFilterManager& filterManager,
		                    FilterAssistant* filterAssistant,
		                    const std::filesystem::path* workingDirectory)
		{
			std::vector<std::filesystem::path> paths;
			std::vector<std::wstring> seenNames;

			auto add = [&](const std::filesystem::path& path) {
				auto name = ToLowerAscii(path.filename().wstring());
				// Never instrument our own injected helper. Without a --modules
				// filter it is discovered from the exe's directory like any other
				// module; the helper would then plant int3 across its own code
				// (handler included) and self-trap. It is a coverage mechanism,
				// not a coverage target.
				if (name == L"vehcoveragehelper.dll")
					return;
				auto isSelected =
				    filterManager.IsModuleSelected(path.wstring());
				// Feed the assistant every candidate module (selected or not),
				// like the debugger's LoadModule does, so a run that selects no
				// module still advises a --modules filter.
				if (filterAssistant)
					filterAssistant->OnNewModule(path, isSelected);
				if (!isSelected)
					return;
				if (std::find(seenNames.begin(), seenNames.end(), name) !=
				    seenNames.end())
					return;
				seenNames.push_back(name);
				paths.push_back(path);
			};

			add(exePath);

			auto directory = exePath.parent_path();
			std::size_t visited = 0;
			auto scan = [&](auto&& iterator) {
				std::error_code error;
				std::decay_t<decltype(iterator)> end;
				for (; !error && iterator != end; iterator.increment(error))
				{
					if (++visited > MaxScannedFiles ||
					    paths.size() >= MaxDiscoveredModules)
						break;
					if (!iterator->is_regular_file(error))
						continue;
					auto extension =
					    ToLowerAscii(iterator->path().extension().wstring());
					if (extension == L".dll" || extension == L".exe")
						add(iterator->path());
				}
			};

			// The executable's own directory first, so that when the same DLL
			// name exists in a subdirectory too, the copy next to the exe (the
			// one most likely to be the loaded one) wins the base-name dedup.
			std::error_code error;
			scan(std::filesystem::directory_iterator{
			    directory,
			    std::filesystem::directory_options::skip_permission_denied,
			    error});
			scan(std::filesystem::recursive_directory_iterator{
			    directory,
			    std::filesystem::directory_options::skip_permission_denied,
			    error});

			// Wrapper launchers (ctest, test runners, scripts) live outside the
			// covered project: the interesting modules are the CHILD processes'
			// binaries, which are nowhere near the wrapper's directory. Their
			// natural anchor is the run's --working_dir (e.g. the build tree),
			// so scan it too - the base-name dedup and the caps above apply
			// unchanged. Without this, a `TaleaCppCovApp -- ctest ...` run
			// silently instruments nothing.
			if (workingDirectory && !workingDirectory->empty())
			{
				auto normalizedWorking =
				    std::filesystem::weakly_canonical(*workingDirectory, error);
				auto normalizedExeDir =
				    std::filesystem::weakly_canonical(directory, error);
				if (normalizedWorking != normalizedExeDir)
				{
					scan(std::filesystem::directory_iterator{
					    *workingDirectory,
					    std::filesystem::directory_options::
					        skip_permission_denied,
					    error});
					scan(std::filesystem::recursive_directory_iterator{
					    *workingDirectory,
					    std::filesystem::directory_options::
					        skip_permission_denied,
					    error});
				}
			}

			return paths;
		}

		//---------------------------------------------------------------------
		// Host-wide caches for the process tree. A ctest-style run spawns
		// hundreds of children that all load the same modules, and probe
		// computation (PDB parse + full-function disassembly) is by far the
		// dominant per-child setup cost - recomputing it per child blew the
		// helper hook's ChildReadyWaitTimeoutMs, silently resuming children
		// UNINSTRUMENTED. A module's probes depend only on the module file,
		// so compute once and share; ditto the exe-directory/working-dir
		// discovery scans. Keyed by lowercased paths. Mutex-guarded: the root
		// populates on the main thread, children on the registration thread.
		using SharedModuleProbes = std::shared_ptr<const ModuleProbes>;
		struct ProbeCache
		{
			std::mutex mutex_;
			// null value = computed, module has no probes (not retried).
			std::map<std::wstring, SharedModuleProbes> probesByModule_;
			std::map<std::wstring, std::vector<std::filesystem::path>>
			    scansByRoots_;
		};

		//---------------------------------------------------------------------
		// Keyed by (exe directory, working directory), not the exe path: any
		// exe under a scanned root is itself rediscovered by the scan, so two
		// roots spawned from the same directories yield the same module set.
		std::vector<std::filesystem::path>
		DiscoverModulePathsCached(ProbeCache& cache,
		                          const std::filesystem::path& exePath,
		                          ICoverageFilterManager& filterManager,
		                          FilterAssistant* filterAssistant,
		                          const std::filesystem::path* workingDirectory)
		{
			auto key =
			    ToLowerAscii(exePath.parent_path().wstring()) + L"|" +
			    (workingDirectory ? ToLowerAscii(workingDirectory->wstring())
			                      : L"");
			{
				std::lock_guard<std::mutex> lock{cache.mutex_};
				auto found = cache.scansByRoots_.find(key);
				if (found != cache.scansByRoots_.end())
					return found->second;
			}
			auto paths = DiscoverModulePaths(
			    exePath, filterManager, filterAssistant, workingDirectory);
			std::lock_guard<std::mutex> lock{cache.mutex_};
			return cache.scansByRoots_.emplace(std::move(key), std::move(paths))
			    .first->second;
		}

		//---------------------------------------------------------------------
		// A PE executable can never be loaded into another process as a
		// library, so a foreign .exe's probes in a target's shared section are
		// pure dead weight - with a test-suite build tree (~50 sibling test
		// exes, each statically linking the project core), they dwarfed every
		// section. Keep the target's own exe and every non-exe module.
		bool CanLoadInto(const std::filesystem::path& modulePath,
		                 const std::filesystem::path& targetExePath)
		{
			if (ToLowerAscii(modulePath.extension().wstring()) != L".exe")
				return true;
			return ToLowerAscii(modulePath.wstring()) ==
			       ToLowerAscii(targetExePath.wstring());
		}

		//---------------------------------------------------------------------
		SharedModuleProbes
		ComputeModuleProbesCached(ProbeCache& cache,
		                          const std::filesystem::path& modulePath,
		                          ICoverageFilterManager& filterManager,
		                          const RunCoverageSettings& settings,
		                          std::shared_ptr<PdbLineTableCache> pdbLineTableCache,
		                          FilterAssistant* filterAssistant)
		{
			auto key = ToLowerAscii(modulePath.wstring());
			{
				std::lock_guard<std::mutex> lock{cache.mutex_};
				auto found = cache.probesByModule_.find(key);
				if (found != cache.probesByModule_.end())
					return found->second;
			}
			SharedModuleProbes shared;
			try
			{
				auto probes = ComputeModuleProbes(modulePath,
				                                  filterManager,
				                                  settings,
				                                  pdbLineTableCache,
				                                  filterAssistant);
				if (probes)
					shared = std::make_shared<const ModuleProbes>(
					    std::move(*probes));
			}
			catch (const std::exception& exception)
			{
				// Keep the host alive and the failure attributed: one broken
				// PDB must cost that module's coverage, not the whole run.
				LOG_ERROR("VEH: probe computation failed for {}: {}",
				          Tools::ToLocalString(modulePath.wstring()),
				          exception.what());
			}
			std::lock_guard<std::mutex> lock{cache.mutex_};
			return cache.probesByModule_.emplace(std::move(key), std::move(shared))
			    .first->second;
		}

		//---------------------------------------------------------------------
		std::vector<wchar_t>
		BuildCommandLine(const std::vector<std::wstring>& arguments)
		{
			std::vector<wchar_t> buffer;
			for (const auto& argument : arguments)
			{
				buffer.push_back(L'"');
				buffer.insert(buffer.end(), argument.begin(), argument.end());
				buffer.push_back(L'"');
				buffer.push_back(L' ');
			}
			buffer.push_back(L'\0');
			return buffer;
		}

		//---------------------------------------------------------------------
		bool Is64BitProcess(HANDLE hProcess)
		{
			BOOL isWow64 = FALSE;
			if (!IsWow64Process(hProcess, &isWow64))
				return true;
			return !isWow64;
		}

		//---------------------------------------------------------------------
		// Loads the helper DLL into the target by running LoadLibraryW there.
		// kernel32 is mapped at the same address in every same-bitness process
		// of the session, so its LoadLibraryW address is valid cross-process.
		bool InjectHelper(HANDLE hProcess,
		                  const std::filesystem::path& helperDllPath)
		{
			auto pathString = helperDllPath.wstring();
			auto byteSize = (pathString.size() + 1) * sizeof(wchar_t);

			auto* remoteMemory = VirtualAllocEx(
			    hProcess, nullptr, byteSize, MEM_COMMIT, PAGE_READWRITE);
			if (!remoteMemory)
			{
				LOG_ERROR("VEH: cannot allocate memory in the target.");
				return false;
			}

			bool success = false;
			if (WriteProcessMemory(hProcess,
			                       remoteMemory,
			                       pathString.c_str(),
			                       byteSize,
			                       nullptr))
			{
				auto* kernel32 = GetModuleHandleW(L"kernel32.dll");
				auto loadLibrary =
				    kernel32 ? GetProcAddress(kernel32, "LoadLibraryW")
				             : nullptr;
				if (loadLibrary)
				{
					auto thread = CreateRemoteThread(
					    hProcess,
					    nullptr,
					    0,
					    reinterpret_cast<LPTHREAD_START_ROUTINE>(loadLibrary),
					    remoteMemory,
					    0,
					    nullptr);
					if (thread)
					{
						WaitForSingleObject(thread, INFINITE);
						// The remote LoadLibraryW returns the loaded HMODULE
						// (truncated to 32 bits in the thread exit code); 0 means
						// the helper failed to load. Checking it makes a failed
						// injection return promptly instead of letting
						// WaitForHelperReady poll the full timeout.
						DWORD threadExit = 0;
						GetExitCodeThread(thread, &threadExit);
						CloseHandle(thread);
						success = threadExit != 0;
					}
				}
			}

			if (!success)
				LOG_ERROR("VEH: cannot inject the coverage helper.");

			VirtualFreeEx(hProcess, remoteMemory, 0, MEM_RELEASE);
			return success;
		}

		//---------------------------------------------------------------------
		bool WaitForHelperReady(volatile VehCoverage::SharedHeader* header)
		{
			auto deadline = GetTickCount64() + HelperReadyTimeoutMs;
			for (;;)
			{
				auto status = header->status;
				if (status == static_cast<std::uint32_t>(
				                  VehCoverage::HelperStatus::Ready))
					return true;
				if (status == static_cast<std::uint32_t>(
				                  VehCoverage::HelperStatus::Error))
				{
					LOG_ERROR("VEH: helper reported error code {}.", header->helperErrorCode);
					return false;
				}
				if (GetTickCount64() >= deadline)
				{
					LOG_ERROR("VEH: timed out waiting for the helper.");
					return false;
				}
				Sleep(1);
			}
		}

		//---------------------------------------------------------------------
		// Lays out the shared section: header, one descriptor per module, then
		// the rva and hit arrays. Returns the mapping handle and mapped view.
		bool CreateSharedSection(DWORD processId,
		                         const std::vector<SharedModuleProbes>& modules,
		                         bool coverChildren,
		                         HANDLE& mappingOut,
		                         void*& viewOut)
		{
			std::size_t totalSize =
			    sizeof(VehCoverage::SharedHeader) +
			    VehCoverage::DescriptorsSize(
			        static_cast<std::uint32_t>(modules.size()));
			for (const auto& module : modules)
			{
				totalSize += VehCoverage::ModuleRegionSize(
				    static_cast<std::uint32_t>(module->probeRvas_.size()),
				    static_cast<std::uint32_t>(module->branches_.size()));
			}

			auto mappingName = std::format(
			    L"{}{}", VehCoverage::MappingNamePrefix, processId);
			auto mapping = CreateFileMappingW(
			    INVALID_HANDLE_VALUE,
			    nullptr,
			    PAGE_READWRITE,
			    static_cast<DWORD>(
			        static_cast<std::uint64_t>(totalSize) >> 32),
			    static_cast<DWORD>(totalSize & 0xFFFFFFFF),
			    mappingName.c_str());
			auto* view =
			    mapping ? MapViewOfFile(mapping, FILE_MAP_ALL_ACCESS, 0, 0, 0)
			            : nullptr;
			if (!view)
			{
				if (mapping)
					CloseHandle(mapping);
				return false;
			}

			auto* header = static_cast<VehCoverage::SharedHeader*>(view);
			header->magic = VehCoverage::ProtocolMagic;
			header->version = VehCoverage::ProtocolVersion;
			header->status = static_cast<std::uint32_t>(
			    VehCoverage::HelperStatus::Initializing);
			header->moduleCount =
			    static_cast<std::uint32_t>(modules.size());
			header->helperErrorCode = 0;
			header->reserved = 0;
			header->coverChildren = coverChildren ? 1 : 0;
			header->childLock = 0;
			header->childPid = 0;
			header->stopChildThread = 0;

			auto* descriptors = VehCoverage::GetModuleDescriptors(header);
			auto* base = static_cast<std::uint8_t*>(view);
			auto arrayOffset =
			    sizeof(VehCoverage::SharedHeader) +
			    VehCoverage::DescriptorsSize(
			        static_cast<std::uint32_t>(modules.size()));

			for (std::size_t i = 0; i < modules.size(); ++i)
			{
				const auto& module = *modules[i];
				auto& descriptor = descriptors[i];
				auto probeCount =
				    static_cast<std::uint32_t>(module.probeRvas_.size());

				std::fill(std::begin(descriptor.baseName_),
				          std::end(descriptor.baseName_),
				          L'\0');
				std::fill(std::begin(descriptor.loadedPath_),
				          std::end(descriptor.loadedPath_),
				          L'\0');
				auto count = std::min(module.baseNameLower_.size(),
				                      VehCoverage::MaxModuleNameChars - 1);
				std::copy_n(module.baseNameLower_.begin(),
				            count,
				            descriptor.baseName_);

				auto branchCount =
				    static_cast<std::uint32_t>(module.branches_.size());

				descriptor.probeCount_ = probeCount;
				descriptor.rvaOffset_ =
				    static_cast<std::uint32_t>(arrayOffset);
				descriptor.hitOffset_ = static_cast<std::uint32_t>(
				    arrayOffset + probeCount * sizeof(std::uint32_t));
				descriptor.branchCount_ = branchCount;
				descriptor.branchOffset_ = static_cast<std::uint32_t>(
				    arrayOffset +
				    VehCoverage::AlignUp(
				        VehCoverage::ModuleArraysSize(probeCount),
				        alignof(VehCoverage::BranchProbe)));
				descriptor.loaded_ = 0;

				auto* rvaArray = reinterpret_cast<std::uint32_t*>(
				    base + descriptor.rvaOffset_);
				std::copy(module.probeRvas_.begin(),
				          module.probeRvas_.end(),
				          rvaArray);
				auto* hitArray = base + descriptor.hitOffset_;
				std::fill_n(hitArray, probeCount, std::uint8_t{0});

				auto* branchArray = reinterpret_cast<VehCoverage::BranchProbe*>(
				    base + descriptor.branchOffset_);
				for (std::uint32_t b = 0; b < branchCount; ++b)
				{
					const auto& source = module.branches_[b];
					branchArray[b].rva_ = source.rva_;
					branchArray[b].originalByte_ = source.originalByte_;
					branchArray[b].condition_ =
					    static_cast<std::uint8_t>(source.condition_);
					branchArray[b].outcome_ = 0;
					branchArray[b].reserved_ = 0;
				}

				arrayOffset +=
				    VehCoverage::ModuleRegionSize(probeCount, branchCount);
			}

			mappingOut = mapping;
			viewOut = view;
			return true;
		}

		//---------------------------------------------------------------------
		// Registers a loaded module's probes and executed bits into the manager
		// and returns its CreateCoverageData. Uses a per-module synthetic
		// address tag so equal RVAs in different modules stay distinct keys.
		void RegisterLoadedModule(const ModuleProbes& module,
		                          const wchar_t* loadedPath,
		                          std::uintptr_t addressTag,
		                          const std::uint8_t* hits,
		                          ExecutedAddressManager& manager)
		{
			// Prefer the path the helper reported (the actually-loaded file)
			// over the copy the directory scan happened to find.
			std::wstring modulePath =
			    (loadedPath && loadedPath[0]) ? loadedPath
			                                  : module.path_.wstring();
			manager.AddModule(modulePath, nullptr);
			for (std::size_t i = 0; i < module.probeRvas_.size(); ++i)
			{
				Address address{
				    reinterpret_cast<HANDLE>(addressTag),
				    reinterpret_cast<void*>(
				        static_cast<std::uintptr_t>(module.probeRvas_[i]))};
				for (const auto& [fileIndex, lineNumber] : module.probeLines_[i])
				{
					// AddModule above sets the last module, so this cannot fail;
					// bridge the invariant back to a throw for the VEH path.
					[[maybe_unused]] auto kept = Tools::ValueOrThrow(
					    manager.RegisterAddress(
					        address, module.files_[fileIndex], lineNumber, 0));
				}
				if (hits[i])
					manager.MarkAddressAsExecuted(address);
			}
		}

		//---------------------------------------------------------------------
		// A branch outcome read back from the shared section, tagged with where
		// it belongs in the coverage data.
		struct BranchResult
		{
			std::wstring modulePath_;
			std::wstring filePath_;
			std::uint32_t lineNumber_;
			std::uint32_t branchIndex_;
			std::uint8_t outcome_;
		};

		//---------------------------------------------------------------------
		// Merges the process tree's coverage incrementally so a child's shared
		// section can be released the moment its process exits. Keeping every
		// child's section mapped until the end - and registering every child's
		// modules separately - made host memory grow with the child count: a
		// 1655-test ctest run exhausted system commit, killing the host with
		// bad_alloc and starving the tests themselves into spurious bad_allocs
		// and SEGFAULTs. One accumulator per module ORs in each process's hit
		// and branch-outcome bytes; the merged result is registered exactly
		// once, in Finalize.
		class CoverageCollector
		{
		  public:
			// Reads one process's section; the caller unmaps it afterwards.
			// Called from the registration thread (reaped children) and the
			// main thread (root + stragglers), hence the mutex.
			void Absorb(const std::vector<SharedModuleProbes>& modules,
			            void* view)
			{
				auto* header = static_cast<VehCoverage::SharedHeader*>(view);
				auto* descriptors = VehCoverage::GetModuleDescriptors(header);
				auto* base = static_cast<std::uint8_t*>(view);

				std::lock_guard<std::mutex> lock{mutex_};
				for (std::size_t i = 0; i < modules.size(); ++i)
				{
					const auto& descriptor = descriptors[i];
					if (!descriptor.loaded_)
						continue;
					auto& accumulator = FindOrCreate(modules[i], descriptor);
					const auto* hits = base + descriptor.hitOffset_;
					for (std::size_t p = 0; p < accumulator.hits_.size(); ++p)
						accumulator.hits_[p] |= hits[p];
					const auto* branchArray =
					    reinterpret_cast<const VehCoverage::BranchProbe*>(
					        base + descriptor.branchOffset_);
					for (std::size_t b = 0;
					     b < accumulator.branchOutcomes_.size();
					     ++b)
						accumulator.branchOutcomes_[b] |=
						    branchArray[b].outcome_;
				}
			}

			// Registers the merged coverage. Once, after every section has
			// been absorbed.
			void Finalize(ExecutedAddressManager& manager,
			              std::vector<BranchResult>& branchResults)
			{
				std::lock_guard<std::mutex> lock{mutex_};
				// A distinct address tag per module keeps equal RVAs in
				// different modules separate keys in the manager.
				std::uintptr_t addressTag = 1;
				for (auto& [key, accumulator] : accumulators_)
				{
					RegisterLoadedModule(*accumulator.probes_,
					                     accumulator.loadedPath_.c_str(),
					                     addressTag++,
					                     accumulator.hits_.data(),
					                     manager);
					std::wstring modulePath =
					    accumulator.loadedPath_.empty()
					        ? accumulator.probes_->path_.wstring()
					        : accumulator.loadedPath_;
					const auto& branches = accumulator.probes_->branches_;
					for (std::size_t b = 0; b < branches.size(); ++b)
					{
						const auto& branch = branches[b];
						branchResults.push_back(BranchResult{
						    modulePath,
						    accumulator.probes_->files_[branch.fileIndex_],
						    branch.lineNumber_,
						    branch.branchIndex_,
						    accumulator.branchOutcomes_[b]});
					}
				}
			}

		  private:
			struct Accumulator
			{
				SharedModuleProbes probes_;
				std::wstring loadedPath_; // first helper-reported path wins
				std::vector<std::uint8_t> hits_;
				std::vector<std::uint8_t> branchOutcomes_;
			};

			Accumulator&
			FindOrCreate(const SharedModuleProbes& probes,
			             const VehCoverage::ModuleDescriptor& descriptor)
			{
				auto key = ToLowerAscii(probes->path_.wstring());
				auto [found, inserted] =
				    accumulators_.emplace(std::move(key), Accumulator{});
				auto& accumulator = found->second;
				if (inserted)
				{
					accumulator.probes_ = probes;
					accumulator.hits_.resize(probes->probeRvas_.size(), 0);
					accumulator.branchOutcomes_.resize(
					    probes->branches_.size(), 0);
				}
				if (accumulator.loadedPath_.empty() &&
				    descriptor.loadedPath_[0])
					accumulator.loadedPath_ = descriptor.loadedPath_;
				return accumulator;
			}

			std::mutex mutex_;
			std::map<std::wstring, Accumulator> accumulators_;
		};

		//---------------------------------------------------------------------
		// Attaches the recorded branch outcomes to the built coverage data,
		// matching the debugger's BranchCoverageManager::DecorateCoverageData.
		void DecorateBranches(Coverage::CoverageData& coverageData,
		                      const std::vector<BranchResult>& results)
		{
			if (results.empty())
				return;

			std::unordered_map<
			    std::wstring,
			    std::unordered_map<std::wstring, Coverage::FileCoverage*>>
			    filesByModule;
			for (const auto& module : coverageData.GetModules())
			{
				auto& files = filesByModule[module->GetPath().wstring()];
				for (const auto& file : module->GetFiles())
					files[file->GetPath().wstring()] = file.get();
			}

			for (const auto& result : results)
			{
				auto moduleIt = filesByModule.find(result.modulePath_);
				if (moduleIt == filesByModule.end())
					continue;
				auto fileIt = moduleIt->second.find(result.filePath_);
				if (fileIt == moduleIt->second.end())
					continue;

				auto* file = fileIt->second;
				if (!(*file)[result.lineNumber_])
					continue; // the line was filtered out; skip its branch

				file->AddBranch(
				    result.lineNumber_,
				    Coverage::BranchCoverage{
				        result.branchIndex_,
				        (result.outcome_ & VehCoverage::BranchTaken) != 0,
				        (result.outcome_ & VehCoverage::BranchFallThrough) !=
				            0});
			}
		}

		//---------------------------------------------------------------------
		// A child process the host injected into, held until its coverage is
		// read back after it exits. pending/ready are this child's own
		// registration events when it in turn covers its children.
		// RAII for the Win32 resources released on RunCoverage's many exit
		// paths: a HANDLE (CloseHandle) and a mapped section view
		// (UnmapViewOfFile). unique_ptr's deleter runs only when non-null, so
		// empty (nullptr) resources are ignored. Both are move-only.
		struct HandleDeleter
		{
			void operator()(void* h) const noexcept { CloseHandle(h); }
		};
		struct ViewDeleter
		{
			void operator()(void* v) const noexcept { UnmapViewOfFile(v); }
		};
		using UniqueHandle = std::unique_ptr<void, HandleDeleter>;
		using UniqueView = std::unique_ptr<void, ViewDeleter>;

		struct ChildCoverage
		{
			UniqueHandle process;
			UniqueHandle mapping;
			UniqueView view;
			std::vector<SharedModuleProbes> modules;
			UniqueHandle pending;
			UniqueHandle ready;
		};

		//---------------------------------------------------------------------
		// Absorbs and releases every tracked child whose process has exited.
		// Mapping a section only while its process lives is what keeps host
		// memory flat across ctest-scale child counts. The caller must have
		// dropped any RegSlot aliasing the child's handles/view first.
		void ReapExitedChildren(std::vector<ChildCoverage>& children,
		                        CoverageCollector& collector)
		{
			for (auto& child : children)
			{
				if (!child.process)
					continue; // already reaped
				if (WaitForSingleObject(child.process.get(), 0) !=
				    WAIT_OBJECT_0)
					continue;
				if (child.view)
					collector.Absorb(child.modules, child.view.get());
				child.view.reset();
				child.mapping.reset();
				child.pending.reset();
				child.ready.reset();
				child.process.reset();
				child.modules.clear();
				child.modules.shrink_to_fit();
			}
		}

		//---------------------------------------------------------------------
		HANDLE MakeChildEvent(const wchar_t* prefix, DWORD pid)
		{
			auto name = std::format(L"{}{}", prefix, pid);
			return CreateEventW(nullptr, FALSE, FALSE, name.c_str());
		}

		//---------------------------------------------------------------------
		// A job that suppresses the Windows Error Reporting dialog for the
		// whole target process tree. The debugger engine never triggers WER
		// (an attached debugger takes the target's unhandled exceptions); the
		// VEH engine runs the target natively, so a crashing test would raise
		// the "stopped working" box - blocking an unattended run and hiding
		// the failure behind a dialog. JOB_OBJECT_LIMIT_DIE_ON_UNHANDLED_
		// EXCEPTION forces SEM_NOGPFAULTERRORBOX on every process in the job,
		// so a crash terminates silently and its exception code surfaces as
		// the exit code (which ctest and this engine log). Child processes
		// inherit job membership, so ctest's spawned test exes are covered.
		//
		// Returns the job handle (assign the root process to it before
		// resuming), or null if job setup failed - in which case the run
		// still proceeds, just without dialog suppression.
		HANDLE CreateNoWerDialogJob()
		{
			HANDLE job = CreateJobObjectW(nullptr, nullptr);
			if (!job)
				return nullptr;

			JOBOBJECT_EXTENDED_LIMIT_INFORMATION info{};
			info.BasicLimitInformation.LimitFlags =
			    JOB_OBJECT_LIMIT_DIE_ON_UNHANDLED_EXCEPTION;
			// Deliberately NOT KILL_ON_JOB_CLOSE: children are read back for
			// coverage after they exit on their own; the job only shapes
			// error handling, it must not reap live processes.
			if (!SetInformationJobObject(job,
			                             JobObjectExtendedLimitInformation,
			                             &info,
			                             sizeof(info)))
			{
				CloseHandle(job);
				return nullptr;
			}
			return job;
		}

		//---------------------------------------------------------------------
		// Sets up a child the parent's hook force-suspended: compute its probes
		// offline, lay out its shared section, and inject the helper into the
		// still-suspended child. When coverChildren is set the child gets its
		// own registration events so its grandchildren are covered too. The
		// parent's hook resumes it once the host releases the ready event.
		void SetupChildProcess(DWORD childPid,
		                       ICoverageFilterManager& filterManager,
		                       const RunCoverageSettings& settings,
		                       std::shared_ptr<PdbLineTableCache> pdbLineTableCache,
		                       const std::filesystem::path& helperDllPath,
		                       bool coverChildren,
		                       ProbeCache& probeCache,
		                       ChildCoverage& out)
		{
			// The parent's hook resumes the child after ChildReadyWaitTimeoutMs
			// no matter what, so setup slower than that means an uninstrumented
			// child. Measure and report it - a silent gap here cost a whole
			// run's coverage once.
			auto setupStart = GetTickCount64();

			out.process.reset(OpenProcess(PROCESS_ALL_ACCESS, FALSE, childPid));
			if (!out.process)
				return;
			if (!Is64BitProcess(out.process.get()))
				return; // VEH mode is 64-bit only

			wchar_t exePath[MAX_PATH];
			DWORD size = MAX_PATH;
			if (!QueryFullProcessImageNameW(out.process.get(), 0, exePath, &size))
				return;

			for (const auto& modulePath : DiscoverModulePathsCached(
			         probeCache,
			         exePath,
			         filterManager,
			         nullptr,
			         settings.GetStartInfo().GetWorkingDirectory()))
			{
				if (!CanLoadInto(modulePath, exePath))
					continue;
				auto probes = ComputeModuleProbesCached(probeCache,
				                                        modulePath,
				                                        filterManager,
				                                        settings,
				                                        pdbLineTableCache,
				                                        nullptr);
				if (probes)
					out.modules.push_back(std::move(probes));
			}

			auto setupElapsed = GetTickCount64() - setupStart;
			if (setupElapsed > VehCoverage::ChildReadyWaitTimeoutMs)
			{
				LOG_ERROR(
				    "VEH: child process {} setup took {} ms, beyond the {} ms "
				    "its parent's hook waits - the child has resumed "
				    "UNINSTRUMENTED and its coverage is lost.",
				    childPid,
				    setupElapsed,
				    VehCoverage::ChildReadyWaitTimeoutMs);
			}

			if (out.modules.empty())
				return;

			HANDLE mapping = nullptr;
			void* view = nullptr;
			if (!CreateSharedSection(
			        childPid, out.modules, coverChildren, mapping, view))
				return;
			out.mapping.reset(mapping);
			out.view.reset(view);

			// The child's helper opens these in its own Initialize, so they
			// must exist before it is injected.
			if (coverChildren)
			{
				out.pending.reset(MakeChildEvent(
				    VehCoverage::ChildPendingEventPrefix, childPid));
				out.ready.reset(MakeChildEvent(
				    VehCoverage::ChildReadyEventPrefix, childPid));
			}

			if (InjectHelper(out.process.get(), helperDllPath))
				WaitForHelperReady(
				    static_cast<VehCoverage::SharedHeader*>(out.view.get()));
		}

		//---------------------------------------------------------------------
		// One process being watched for child spawns: its section header (the
		// childPid slot) and its registration event pair.
		struct RegSlot
		{
			VehCoverage::SharedHeader* header;
			HANDLE pending;
			HANDLE ready;
			HANDLE process; // to prune the slot once the process exits
		};

		//---------------------------------------------------------------------
		// Single registration loop for the whole process tree. Waits on the
		// pending events of every injected process at once; when one fires, it
		// injects into that process's freshly suspended child, and - if that
		// child covers children too - adds the child as another watched slot,
		// so grandchildren are handled by the same loop. stopEvent breaks it.
		void RunRegistrationLoop(
		    HANDLE stopEvent,
		    const RegSlot& root,
		    ICoverageFilterManager& filterManager,
		    const RunCoverageSettings& settings,
		    std::shared_ptr<PdbLineTableCache> pdbLineTableCache,
		    const std::filesystem::path& helperDllPath,
		    ProbeCache& probeCache,
		    CoverageCollector& collector,
		    std::vector<ChildCoverage>& children)
		{
			std::vector<RegSlot> slots;
			slots.push_back(root);

			// Exited processes spawn nothing anymore: their slots only waste
			// wait capacity (the MAXIMUM_WAIT_OBJECTS cap), and their backing
			// ChildCoverage pins a mapped section. Prune the slots first (they
			// alias the child's handles/view), then reap the children.
			auto pruneAndReap = [&] {
				slots.erase(
				    std::remove_if(
				        slots.begin(),
				        slots.end(),
				        [](const RegSlot& slot) {
					        return WaitForSingleObject(slot.process, 0) ==
					               WAIT_OBJECT_0;
				        }),
				    slots.end());
				ReapExitedChildren(children, collector);
			};

			bool warnedCap = false;
			std::vector<HANDLE> waitHandles;
			for (;;)
			{
				waitHandles.clear();
				waitHandles.push_back(stopEvent);
				for (const auto& slot : slots)
				{
					if (waitHandles.size() >= MAXIMUM_WAIT_OBJECTS)
						break;
					waitHandles.push_back(slot.pending);
				}

				// A finite wait so exited children are reaped promptly even
				// when nothing spawns.
				auto result = WaitForMultipleObjects(
				    static_cast<DWORD>(waitHandles.size()),
				    waitHandles.data(),
				    FALSE,
				    5000);
				if (result == WAIT_TIMEOUT)
				{
					pruneAndReap();
					continue;
				}
				if (result < WAIT_OBJECT_0 + 1 ||
				    result >= WAIT_OBJECT_0 + waitHandles.size())
					break; // stopEvent (index 0) or a wait failure

				auto& slot = slots[(result - WAIT_OBJECT_0) - 1];
				ChildCoverage child;
				SetupChildProcess(slot.header->childPid,
				                  filterManager,
				                  settings,
				                  pdbLineTableCache,
				                  helperDllPath,
				                  /*coverChildren*/ true,
				                  probeCache,
				                  child);

				if (child.pending && child.ready && child.view)
				{
					if (slots.size() + 1 < MAXIMUM_WAIT_OBJECTS)
						// Non-owning aliases of the handles owned by `child`;
						// captured before child is moved into `children` below.
						slots.push_back(
						    {static_cast<VehCoverage::SharedHeader*>(
						         child.view.get()),
						     child.pending.get(),
						     child.ready.get(),
						     child.process.get()});
					else if (!warnedCap)
					{
						LOG_WARNING("VEH: watching the maximum number of processes for child spawns; deeper descendants are not instrumented.");
						warnedCap = true;
					}
				}
				children.push_back(std::move(child));
				SetEvent(slot.ready);
				pruneAndReap();
			}
		}
	}

	//-------------------------------------------------------------------------
	VehCoverageEngine::VehCoverageEngine(
	    std::shared_ptr<Tools::WarningManager> warningManager)
	    : warningManager_{std::move(warningManager)},
	      filterAssistant_{std::make_shared<FilterAssistant>(
	          std::make_shared<FileSystem>())}
	{
	}

	//-------------------------------------------------------------------------
	VehCoverageEngine::~VehCoverageEngine() = default;

	//-------------------------------------------------------------------------
	std::filesystem::path VehCoverageEngine::GetHelperDllPath()
	{
		HMODULE thisModule = nullptr;
		if (!GetModuleHandleExW(
		        GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
		            GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
		        reinterpret_cast<LPCWSTR>(&VehCoverageEngine::GetHelperDllPath),
		        &thisModule))
		{
			return L"VehCoverageHelper.dll";
		}

		wchar_t modulePath[MAX_PATH];
		auto length = GetModuleFileNameW(thisModule, modulePath, MAX_PATH);
		if (length == 0 || length == MAX_PATH)
			return L"VehCoverageHelper.dll";

		std::filesystem::path path{modulePath};
		return path.parent_path() / L"VehCoverageHelper.dll";
	}

	//-------------------------------------------------------------------------
	Tools::Expected<Coverage::CoverageData>
	VehCoverageEngine::RunCoverage(const RunCoverageSettings& settings)
	{
		const auto& startInfo = settings.GetStartInfo();
		const auto& exePath = startInfo.GetPath();
		auto coverageName = exePath.filename().wstring();

		CoverageFilterManager filterManager{
		    settings.GetCoverageFilterSettings(),
		    settings.GetUnifiedDiffSettings(),
		    settings.GetExcludedLineRegexes(),
		    /*useReleaseCoverageFilter*/ false};

		std::shared_ptr<PdbLineTableCache> pdbLineTableCache;
		if (settings.IsPdbCacheEnabled())
		{
			pdbLineTableCache = std::make_shared<PdbLineTableCache>(
			    PdbLineTableCache::GetDefaultDiskCacheDirectory());
		}

		// The cache outlives the registration thread: the root populates it
		// here, every child then reuses the probe sets (see ProbeCache).
		ProbeCache probeCache;

		std::vector<SharedModuleProbes> modules;
		for (const auto& modulePath :
		     DiscoverModulePathsCached(probeCache,
		                               exePath,
		                               filterManager,
		                               filterAssistant_.get(),
		                               startInfo.GetWorkingDirectory()))
		{
			if (!CanLoadInto(modulePath, exePath))
				continue;
			auto probes = ComputeModuleProbesCached(probeCache,
			                                        modulePath,
			                                        filterManager,
			                                        settings,
			                                        pdbLineTableCache,
			                                        filterAssistant_.get());
			if (probes)
				modules.push_back(std::move(probes));
		}

		auto commandLine = BuildCommandLine(startInfo.GetArguments());
		const auto* workingDirectory = startInfo.GetWorkingDirectory();

		STARTUPINFOW startupInfo{};
		startupInfo.cb = sizeof(startupInfo);
		PROCESS_INFORMATION processInfo{};
		if (!CreateProcessW(nullptr,
		                    commandLine.data(),
		                    nullptr,
		                    nullptr,
		                    FALSE,
		                    CREATE_SUSPENDED | CREATE_UNICODE_ENVIRONMENT,
		                    nullptr,
		                    workingDirectory ? workingDirectory->c_str()
		                                     : nullptr,
		                    &startupInfo,
		                    &processInfo))
		{
			return std::unexpected(
			    MAKE_ERROR(L"VEH: cannot start " << exePath.wstring()));
		}

		// Own the process/thread handles from here on so every exit path -
		// including the error return below - releases them.
		UniqueHandle processHandle{processInfo.hProcess};
		UniqueHandle threadHandle{processInfo.hThread};

		// Suppress the WER crash dialog for the whole tree before the target
		// runs a single instruction (it is CREATE_SUSPENDED). Best-effort:
		// keep the job handle alive for the run's duration by holding it in
		// scope; assignment failure is non-fatal.
		UniqueHandle werJob{CreateNoWerDialogJob()};
		if (werJob && !AssignProcessToJobObject(werJob.get(),
		                                        processHandle.get()))
		{
			LOG_WARNING("VEH: could not suppress the crash-report dialog for "
			            "the target; a crashing child may show a WER box.");
			werJob.reset();
		}

		if (!Is64BitProcess(processHandle.get()))
		{
			TerminateProcess(processHandle.get(), 1);
			return std::unexpected(
			    MAKE_ERROR(L"VEH mode supports only 64-bit targets."));
		}

		auto coverChildren = settings.GetCoverChildren();
		auto helperDllPath = GetHelperDllPath();

		UniqueHandle mapping;
		UniqueView view;
		UniqueHandle rootPending;
		UniqueHandle rootReady;
		UniqueHandle stopEvent;
		std::thread registrationThread;
		std::vector<ChildCoverage> children;
		CoverageCollector collector;

		if (!modules.empty())
		{
			HANDLE rawMapping = nullptr;
			void* rawView = nullptr;
			if (CreateSharedSection(processInfo.dwProcessId,
			                        modules,
			                        coverChildren,
			                        rawMapping,
			                        rawView))
			{
				mapping.reset(rawMapping);
				view.reset(rawView);
				auto* header =
				    static_cast<VehCoverage::SharedHeader*>(view.get());

				// The registration events must exist before the helper is
				// injected (its InstallChildProcessHook opens them). One loop
				// watches the whole tree (see RunRegistrationLoop).
				if (coverChildren)
				{
					auto rootPid = processInfo.dwProcessId;
					rootPending.reset(MakeChildEvent(
					    VehCoverage::ChildPendingEventPrefix, rootPid));
					rootReady.reset(MakeChildEvent(
					    VehCoverage::ChildReadyEventPrefix, rootPid));
					stopEvent.reset(
					    CreateEventW(nullptr, TRUE, FALSE, nullptr));
					if (rootPending && rootReady && stopEvent)
					{
						RegSlot rootSlot{header,
						                 rootPending.get(),
						                 rootReady.get(),
						                 processHandle.get()};
						auto* stop = stopEvent.get();
						registrationThread = std::thread([&, rootSlot, stop] {
							RunRegistrationLoop(stop,
							                    rootSlot,
							                    filterManager,
							                    settings,
							                    pdbLineTableCache,
							                    helperDllPath,
							                    probeCache,
							                    collector,
							                    children);
						});
					}
					else
					{
						// Without the registration events there is no host to
						// service children, so disable child hooking; otherwise
						// every spawned child would block waiting on a ready
						// signal that never comes. The header is read by the
						// helper only after injection below.
						LOG_WARNING("VEH: child-process registration unavailable; child coverage disabled.");
						header->coverChildren = 0;
						coverChildren = false;
					}
				}

				if (InjectHelper(processHandle.get(), helperDllPath))
					WaitForHelperReady(header);
				else
					LOG_WARNING("VEH: continuing without instrumentation.");
			}
			else
			{
				LOG_ERROR("VEH: cannot create the shared section; running without instrumentation.");
			}
		}

		ResumeThread(threadHandle.get());
		WaitForSingleObject(processHandle.get(), INFINITE);

		// Stop the registration loop now that the whole tree rooted here has
		// exited (children were waited on by their parents).
		if (registrationThread.joinable())
		{
			SetEvent(stopEvent.get());
			registrationThread.join();
		}

		DWORD exitCode = 0;
		GetExitCodeProcess(processHandle.get(), &exitCode);

		// A crash exit code is now silent at the OS level (the WER dialog is
		// suppressed), so surface it in the log - the run must never hide a
		// target that died. The high bit marks NTSTATUS failure codes
		// (0xC0000005 access violation, 0xC0000409 fast-fail, ...).
		if ((exitCode & 0xC0000000u) == 0xC0000000u)
		{
			LOG_WARNING(
			    "VEH: the target exited with crash code {:#010x}; its coverage "
			    "up to the fault is included.",
			    exitCode);
		}

		ExecutedAddressManager manager;
		std::vector<BranchResult> branchResults;

		if (view)
			collector.Absorb(modules, view.get());

		// Most children were absorbed and released the moment they exited
		// (see ReapExitedChildren); only stragglers still alive when the loop
		// stopped remain. Bounded wait: a leaked grandchild must not hang the
		// host - the section stays readable even while its process lives.
		for (auto& child : children)
		{
			if (!child.process)
				continue; // reaped during the run
			if (WaitForSingleObject(child.process.get(), 30000) ==
			    WAIT_TIMEOUT)
			{
				LOG_WARNING(
				    "VEH: a covered process is still running after the "
				    "target exited; reading its coverage as-is.");
			}
			if (child.view)
				collector.Absorb(child.modules, child.view.get());
			child.view.reset();
			child.mapping.reset();
			child.pending.reset();
			child.ready.reset();
			child.process.reset();
		}

		collector.Finalize(manager, branchResults);

		auto coverageData = manager.CreateCoverageData(
		    coverageName, static_cast<std::int32_t>(exitCode));
		DecorateBranches(coverageData, branchResults);

		// Same filter advice the debugger engine emits: the assistant was fed
		// every candidate module and source during offline probe discovery.
		auto filterAdviceMessage = filterAssistant_->GetAdviceMessage();
		if (filterAdviceMessage)
			warningManager_->AddWarning(*filterAdviceMessage);

		return coverageData;
	}
}
