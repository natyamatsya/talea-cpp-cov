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
#include "VehCoverageEngine.hpp"

#include <Windows.h>

#include <algorithm>
#include <format>
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

#include "tools/Log.hpp"
#include "Tools/WarningManager.hpp"

#include "Address.hpp"
#include "BranchDisassembler.hpp"
#include "BranchInstruction.hpp"
#include "CppCoverageException.hpp"
#include "CoverageFilterManager.hpp"
#include "DebugInformationEnumerator.hpp"
#include "ExecutedAddressManager.hpp"
#include "LineRunScanner.hpp"
#include "PdbLineTableCache.hpp"
#include "PeImageFileReader.hpp"
#include "RunCoverageSettings.hpp"
#include "StartInfo.hpp"

#include "FileFilter/ModuleInfo.hpp"
#include "FileFilter/FileInfo.hpp"
#include "FileFilter/LineInfo.hpp"

#include "Plugin/Exporter/CoverageData.hpp"
#include "Plugin/Exporter/ModuleCoverage.hpp"
#include "Plugin/Exporter/FileCoverage.hpp"
#include "Plugin/Exporter/LineCoverage.hpp"

#include "VehCoverageHelper/VehProtocol.hpp"
#include "VehCoverageHelper/VehBranchEval.hpp"

namespace CppCoverage
{
	namespace
	{
		constexpr DWORD HelperReadyTimeoutMs = 60000;
		// Bounds on the exe-directory scan, so a pathological tree cannot make
		// startup unbounded.
		constexpr std::size_t MaxDiscoveredModules = 256;
		constexpr std::size_t MaxScannedFiles = 50000;

		//---------------------------------------------------------------------
		// A source line in the module's RVA space.
		struct SelectedLine
		{
			std::uint32_t rva_;
			std::uint32_t length_;
			unsigned int lineNumber_;
			std::wstring filePath_;
		};

		// One instrumented module: the probe leader RVAs the helper patches,
		// and, aligned with them, the (file, line)s each leader covers, so the
		// coverage data can be built from the hit bitmap after the run.
		struct ModuleBranch
		{
			std::uint32_t rva_;
			std::uint8_t originalByte_;
			VehCoverage::BranchCondition condition_;
			std::wstring filePath_;
			unsigned int lineNumber_;
			unsigned int branchIndex_;
		};

		struct ModuleProbes
		{
			std::filesystem::path path_;
			std::wstring baseNameLower_;
			std::vector<std::uint32_t> probeRvas_;
			std::vector<std::vector<std::pair<std::wstring, unsigned int>>>
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
			THROW("Invalid branch condition");
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
			               const std::filesystem::path& modulePath)
			    : filterManager_{filterManager}, modulePath_{modulePath}
			{
			}

			bool IsSourceFileSelected(
			    const std::filesystem::path& path) override
			{
				return filterManager_.IsSourceFileSelected(path.wstring());
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

				const auto& collection = fileInfo.lineInfoColllection_;
				for (std::size_t i = 0; i < collection.size(); ++i)
				{
					const auto& lineInfo = collection[i];
					if (!filterManager_.IsLineSelected(
					        moduleInfo, fileInfo, lineInfo))
						continue;

					selectedLines_.push_back(SelectedLine{
					    static_cast<std::uint32_t>(lineInfo.virtualAddress_),
					    lines[i].length_,
					    static_cast<unsigned int>(lineInfo.lineNumber_),
					    path.wstring()});
				}
			}

			const std::vector<FunctionRange>& GetFunctions() const
			{
				return functions_;
			}
			std::vector<SelectedLine>& GetSelectedLines()
			{
				return selectedLines_;
			}

		  private:
			ICoverageFilterManager& filterManager_;
			const std::filesystem::path modulePath_;
			std::vector<FunctionRange> functions_;
			std::vector<SelectedLine> selectedLines_;
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
						addProbe(lines[cursor].rva_, cursor++);

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
					target.emplace_back(lines[lineIndex].filePath_,
					                    lines[lineIndex].lineNumber_);
			}
		}

		//---------------------------------------------------------------------
		// Disassembles each selected line's code range (from the PE file, no
		// live process) to find its conditional jumps, and records a branch
		// probe per distinct Jcc. Mirrors the debugger's RegisterBranches,
		// minus the process: same Zydis discovery, same per-line branch index.
		void DiscoverBranches(const std::filesystem::path& modulePath,
		                      const std::vector<SelectedLine>& selectedLines,
		                      ModuleProbes& out)
		{
			auto reader = PeImageFileReader::TryCreate(modulePath);
			if (!reader)
				return;

			BranchDisassembler disassembler{/*is64BitCode*/ true};

			std::map<std::pair<std::wstring, unsigned int>, unsigned int>
			    branchIndexByLine;
			std::unordered_set<std::uint32_t> seenRvas; // first Jcc wins

			for (const auto& line : selectedLines)
			{
				if (line.length_ == 0)
					continue;
				auto buffer = reader->ReadCode(line.rva_, line.length_);
				if (buffer.empty())
					continue;

				auto branches = disassembler.FindConditionalBranches(
				    buffer.data(), buffer.size(), line.rva_);
				for (const auto& branch : branches)
				{
					auto rva =
					    static_cast<std::uint32_t>(branch.virtualAddress_);
					if (!seenRvas.insert(rva).second)
						continue;

					auto key =
					    std::make_pair(line.filePath_, line.lineNumber_);
					auto branchIndex = branchIndexByLine[key]++;
					out.branches_.push_back(
					    ModuleBranch{rva,
					                 buffer[rva - line.rva_],
					                 ToVehCondition(branch.condition_),
					                 line.filePath_,
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
		                    std::shared_ptr<PdbLineTableCache> pdbLineTableCache)
		{
			DebugInformationEnumerator enumerator{
			    settings.GetSubstitutePdbSourcePaths(),
			    /*needSymbolIndex*/ false,
			    pdbLineTableCache,
			    /*needFunctionNames*/ false,
			    settings.IsRawPdbEnabled()};
			ProbeCollector collector{filterManager, modulePath};
			if (!enumerator.Enumerate(modulePath, collector))
				return std::nullopt;

			ModuleProbes probes;
			probes.path_ = modulePath;
			probes.baseNameLower_ =
			    ToLowerAscii(modulePath.filename().wstring());
			BuildProbes(modulePath,
			            settings.IsLineMergingEnabled(),
			            collector,
			            probes);
			if (probes.probeRvas_.empty())
				return std::nullopt;

			if (settings.GetBranchCoverage())
				DiscoverBranches(
				    modulePath, collector.GetSelectedLines(), probes);
			return probes;
		}

		//---------------------------------------------------------------------
		// The main executable plus every selected module with a distinct base
		// name found under its directory. Scanning the directory (rather than
		// walking imports) also catches dynamically loaded modules, as long as
		// they live next to the executable.
		std::vector<std::filesystem::path>
		DiscoverModulePaths(const std::filesystem::path& exePath,
		                    ICoverageFilterManager& filterManager)
		{
			std::vector<std::filesystem::path> paths;
			std::vector<std::wstring> seenNames;

			auto add = [&](const std::filesystem::path& path) {
				if (!filterManager.IsModuleSelected(path.wstring()))
					return;
				auto name = ToLowerAscii(path.filename().wstring());
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

			return paths;
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
				LOG_ERROR << L"VEH: cannot allocate memory in the target.";
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
				LOG_ERROR << L"VEH: cannot inject the coverage helper.";

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
					LOG_ERROR << L"VEH: helper reported error code "
					          << header->helperErrorCode << L".";
					return false;
				}
				if (GetTickCount64() >= deadline)
				{
					LOG_ERROR << L"VEH: timed out waiting for the helper.";
					return false;
				}
				Sleep(1);
			}
		}

		//---------------------------------------------------------------------
		// Lays out the shared section: header, one descriptor per module, then
		// the rva and hit arrays. Returns the mapping handle and mapped view.
		bool CreateSharedSection(DWORD processId,
		                         const std::vector<ModuleProbes>& modules,
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
				    static_cast<std::uint32_t>(module.probeRvas_.size()),
				    static_cast<std::uint32_t>(module.branches_.size()));
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
				const auto& module = modules[i];
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
				for (const auto& [filePath, lineNumber] : module.probeLines_[i])
					manager.RegisterAddress(address, filePath, lineNumber, 0);
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
			unsigned int lineNumber_;
			unsigned int branchIndex_;
			std::uint8_t outcome_;
		};

		//---------------------------------------------------------------------
		void CollectBranchResults(const ModuleProbes& module,
		                          const VehCoverage::ModuleDescriptor& descriptor,
		                          const std::uint8_t* base,
		                          std::vector<BranchResult>& out)
		{
			if (descriptor.branchCount_ == 0)
				return;
			std::wstring modulePath =
			    (descriptor.loadedPath_[0]) ? descriptor.loadedPath_
			                                : module.path_.wstring();
			const auto* branchArray =
			    reinterpret_cast<const VehCoverage::BranchProbe*>(
			        base + descriptor.branchOffset_);
			for (std::size_t b = 0; b < module.branches_.size(); ++b)
			{
				// Every discovered branch is reported, even one never reached
				// (outcome 0 => neither direction taken), matching the debugger
				// engine which lists registered branches regardless of hits.
				const auto& branch = module.branches_[b];
				out.push_back(BranchResult{modulePath,
				                           branch.filePath_,
				                           branch.lineNumber_,
				                           branch.branchIndex_,
				                           branchArray[b].outcome_});
			}
		}

		//---------------------------------------------------------------------
		// Attaches the recorded branch outcomes to the built coverage data,
		// matching the debugger's BranchCoverageManager::DecorateCoverageData.
		void DecorateBranches(Plugin::CoverageData& coverageData,
		                      const std::vector<BranchResult>& results)
		{
			if (results.empty())
				return;

			std::unordered_map<
			    std::wstring,
			    std::unordered_map<std::wstring, Plugin::FileCoverage*>>
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
				    Plugin::BranchCoverage{
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
			std::vector<ModuleProbes> modules;
			UniqueHandle pending;
			UniqueHandle ready;
		};

		//---------------------------------------------------------------------
		HANDLE MakeChildEvent(const wchar_t* prefix, DWORD pid)
		{
			auto name = std::format(L"{}{}", prefix, pid);
			return CreateEventW(nullptr, FALSE, FALSE, name.c_str());
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
		                       ChildCoverage& out)
		{
			out.process.reset(OpenProcess(PROCESS_ALL_ACCESS, FALSE, childPid));
			if (!out.process)
				return;
			if (!Is64BitProcess(out.process.get()))
				return; // VEH mode is 64-bit only

			wchar_t exePath[MAX_PATH];
			DWORD size = MAX_PATH;
			if (!QueryFullProcessImageNameW(out.process.get(), 0, exePath, &size))
				return;

			for (const auto& modulePath :
			     DiscoverModulePaths(exePath, filterManager))
			{
				auto probes = ComputeModuleProbes(
				    modulePath, filterManager, settings, pdbLineTableCache);
				if (probes)
					out.modules.push_back(std::move(*probes));
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
		    std::vector<ChildCoverage>& children)
		{
			std::vector<RegSlot> slots;
			slots.push_back(root);

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

				auto result = WaitForMultipleObjects(
				    static_cast<DWORD>(waitHandles.size()),
				    waitHandles.data(),
				    FALSE,
				    INFINITE);
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
						     child.ready.get()});
					else if (!warnedCap)
					{
						LOG_WARNING << L"VEH: watching the maximum number of "
						               L"processes for child spawns; deeper "
						               L"descendants are not instrumented.";
						warnedCap = true;
					}
				}
				children.push_back(std::move(child));
				SetEvent(slot.ready);
			}
		}
	}

	//-------------------------------------------------------------------------
	VehCoverageEngine::VehCoverageEngine(
	    std::shared_ptr<Tools::WarningManager> warningManager)
	    : warningManager_{std::move(warningManager)}
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
	Plugin::CoverageData
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

		std::vector<ModuleProbes> modules;
		for (const auto& modulePath :
		     DiscoverModulePaths(exePath, filterManager))
		{
			auto probes = ComputeModuleProbes(
			    modulePath, filterManager, settings, pdbLineTableCache);
			if (probes)
				modules.push_back(std::move(*probes));
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
			THROW(L"VEH: cannot start " + exePath.wstring());
		}

		// Own the process/thread handles from here on so every exit path -
		// including the throw below - releases them.
		UniqueHandle processHandle{processInfo.hProcess};
		UniqueHandle threadHandle{processInfo.hThread};

		if (!Is64BitProcess(processHandle.get()))
		{
			TerminateProcess(processHandle.get(), 1);
			THROW(L"VEH mode supports only 64-bit targets.");
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
						RegSlot rootSlot{
						    header, rootPending.get(), rootReady.get()};
						auto* stop = stopEvent.get();
						registrationThread = std::thread([&, rootSlot, stop] {
							RunRegistrationLoop(stop,
							                    rootSlot,
							                    filterManager,
							                    settings,
							                    pdbLineTableCache,
							                    helperDllPath,
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
						LOG_WARNING << L"VEH: child-process registration "
						               L"unavailable; child coverage disabled.";
						header->coverChildren = 0;
						coverChildren = false;
					}
				}

				if (InjectHelper(processHandle.get(), helperDllPath))
					WaitForHelperReady(header);
				else
					LOG_WARNING << L"VEH: continuing without instrumentation.";
			}
			else
			{
				LOG_ERROR << L"VEH: cannot create the shared section; "
				             L"running without instrumentation.";
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

		ExecutedAddressManager manager;
		std::vector<BranchResult> branchResults;
		// A distinct address tag per (process, module) keeps equal RVAs in
		// different modules - and the same module in different processes -
		// separate keys in the manager.
		std::uintptr_t addressTag = 1;
		auto readback = [&](const std::vector<ModuleProbes>& mods, void* v) {
			auto* header = static_cast<VehCoverage::SharedHeader*>(v);
			auto* descriptors = VehCoverage::GetModuleDescriptors(header);
			auto* base = static_cast<std::uint8_t*>(v);
			for (std::size_t i = 0; i < mods.size(); ++i)
			{
				// Only modules the helper actually patched contribute, so a
				// precomputed module that never loaded is omitted.
				if (!descriptors[i].loaded_)
					continue;
				auto* hits = base + descriptors[i].hitOffset_;
				RegisterLoadedModule(mods[i],
				                     descriptors[i].loadedPath_,
				                     addressTag++,
				                     hits,
				                     manager);
				CollectBranchResults(
				    mods[i], descriptors[i], base, branchResults);
			}
		};

		if (view)
			readback(modules, view.get());

		// Children were force-suspended and injected as they spawned; read
		// their coverage back once they too have exited. All the child and
		// root handles/views are released automatically when this scope ends
		// (ChildCoverage and the UniqueHandle/UniqueView locals are RAII).
		for (auto& child : children)
		{
			if (child.process)
				WaitForSingleObject(child.process.get(), INFINITE);
			if (child.view)
				readback(child.modules, child.view.get());
		}

		auto coverageData = manager.CreateCoverageData(
		    coverageName, static_cast<int>(exitCode));
		DecorateBranches(coverageData, branchResults);
		return coverageData;
	}
}
