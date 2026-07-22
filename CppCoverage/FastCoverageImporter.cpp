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
#include "FastCoverageImporter.hpp"

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <map>
#include <sstream>
#include <unordered_map>

#ifndef ZYDIS_STATIC_BUILD
#define ZYDIS_STATIC_BUILD
#endif
#include <Zydis/Zydis.h>

#include "Tools/Log.hpp"

#include "Address.hpp"
#include "CppCoverageException.hpp"
#include "CoverageFilterManager.hpp"
#include "CoverageFilterSettings.hpp"
#include "DebugInformationEnumerator.hpp"
#include "ExecutedAddressManager.hpp"
#include "PeImageFileReader.hpp"

#include "FileFilter/ModuleInfo.hpp"
#include "FileFilter/FileInfo.hpp"
#include "FileFilter/LineInfo.hpp"

#include "CoverageRuntime/FastCovFormat.hpp"

namespace CppCoverage
{
	namespace
	{
		//---------------------------------------------------------------------
		struct BasicBlock
		{
			std::uint32_t rva_;
			std::uint32_t count_;
		};

		//---------------------------------------------------------------------
		struct LineRecord
		{
			std::uint32_t rva_;
			std::wstring filePath_;
			std::uint32_t lineNumber_;
		};

		//---------------------------------------------------------------------
		// Collects every selected line record (with its rva) and the function
		// ranges (with demangled names) of one module from its PDB.
		class ModuleCollector : public IDebugInformationHandler
		{
		  public:
			ModuleCollector(ICoverageFilterManager& filterManager,
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
					lineInfos.emplace_back(line.lineNumber_,
					                       line.virtualAddress_,
					                       line.symbolIndex_);

				FileFilter::FileInfo fileInfo{path, std::move(lineInfos)};
				FileFilter::ModuleInfo moduleInfo{nullptr, modulePath_,
				                                  nullptr};

				const auto& collection = fileInfo.lineInfoColllection_;
				for (const auto& lineInfo : collection)
				{
					if (!filterManager_.IsLineSelected(
					        moduleInfo, fileInfo, lineInfo))
						continue;
					records_.push_back(LineRecord{
					    static_cast<std::uint32_t>(lineInfo.virtualAddress_),
					    path.wstring(),
					    static_cast<std::uint32_t>(lineInfo.lineNumber_)});
				}
			}

			std::vector<FunctionRange>& GetFunctions() { return functions_; }
			std::vector<LineRecord>& GetLineRecords() { return records_; }

		  private:
			ICoverageFilterManager& filterManager_;
			const std::filesystem::path modulePath_;
			std::vector<FunctionRange> functions_;
			std::vector<LineRecord> records_;
		};

		//---------------------------------------------------------------------
		// Execution count of the basic block that covers rva: the block with
		// the greatest start address not past rva. 0 when rva precedes every
		// block.
		std::uint32_t CountAt(const std::vector<BasicBlock>& blocks,
		                     std::uint32_t rva)
		{
			auto it = std::upper_bound(
			    blocks.begin(),
			    blocks.end(),
			    rva,
			    [](std::uint32_t value, const BasicBlock& block) {
				    return value < block.rva_;
			    });
			if (it == blocks.begin())
				return 0;
			return (it - 1)->count_;
		}

		//---------------------------------------------------------------------
		// Demangled name of the function containing rva, or empty.
		const std::wstring*
		FunctionNameAt(const std::vector<IDebugInformationHandler::FunctionRange>&
		                   functions,
		               std::uint32_t rva)
		{
			auto it = std::upper_bound(
			    functions.begin(),
			    functions.end(),
			    rva,
			    [](std::uint32_t value, const auto& function) {
				    return value <
				           static_cast<std::uint32_t>(function.virtualAddress_);
			    });
			if (it == functions.begin())
				return nullptr;
			--it;
			auto start = static_cast<std::uint32_t>(it->virtualAddress_);
			if (rva - start >= it->length_)
				return nullptr;
			return &it->name_;
		}

		//---------------------------------------------------------------------
		// The template stem of an instantiation name, or empty when the name is
		// not a template. "foo<int>" -> "foo"; "ns::v<int>::at" -> "ns::v".
		std::wstring TemplateStem(const std::wstring& name)
		{
			auto pos = name.find(L'<');
			if (pos == std::wstring::npos)
				return {};
			auto stem = name.substr(0, pos);
		// operator< / operator<< / operator<= / operator<=> are not templates;
		// their '<' would otherwise yield a bogus "...operator" stem.
		if (stem.ends_with(L"operator"))
			return {};
		return stem;
		}

		//---------------------------------------------------------------------
		std::vector<char> ReadFile(const std::filesystem::path& path)
		{
			std::ifstream ifs{path, std::ios::binary};
			if (!ifs)
				return {};
			return std::vector<char>{std::istreambuf_iterator<char>{ifs},
			                         std::istreambuf_iterator<char>{}};
		}

		//---------------------------------------------------------------------
		// Reconstructs coverage of every basic block of a module from the
		// counters of the subset SanitizerCoverage actually instrumented.
		//
		// Both toolchains use minimal instrumentation: a block whose execution
		// is implied by another (a function entry, a merge, a return) carries
		// no counter. This rebuilds each function's control-flow graph and
		// propagates coverage so line mapping matches the debugger:
		//   - a function's entry dominates every block, so it ran if any block
		//     ran;
		//   - a covered block with a single successor implies that successor
		//     ran (it flows there unconditionally);
		//   - a covered block with a single predecessor implies that
		//     predecessor ran (its only way in).
		// All three are sound for normal control flow; only a path that threw
		// mid-run before a single-successor edge can differ from the debugger
		// (the same limit as the debugger engine's own run merging), and it
		// never over-reports an unexecuted branch.
		//
		// The instrumented counts come from the clang PC table (clangCounts,
		// keyed by block rva) or, for MSVC, from the `inc byte [rip+disp]`
		// increments found while disassembling (region non-null). Functions
		// with an indirect branch (unknown targets) skip propagation and keep
		// only the directly instrumented blocks plus the sound entry cover.
		std::vector<BasicBlock> ReconstructCoverage(
		    PeImageFileReader& reader,
		    const std::vector<IDebugInformationHandler::FunctionRange>&
		        functions,
		    const std::map<std::uint32_t, std::uint32_t>* clangCounts,
		    std::uint32_t regionRva,
		    const std::vector<std::uint8_t>* region)
		{
			std::vector<BasicBlock> blocks;

			ZydisDecoder decoder;
			if (!ZYAN_SUCCESS(ZydisDecoderInit(&decoder,
			                                   ZYDIS_MACHINE_MODE_LONG_64,
			                                   ZYDIS_STACK_WIDTH_64)))
				return blocks;

			std::uint64_t regionEnd =
			    region ? std::uint64_t{regionRva} + region->size() : 0;

			for (const auto& function : functions)
			{
				auto functionRva =
				    static_cast<std::uint32_t>(function.virtualAddress_);
				auto functionEnd = functionRva + function.length_;
				auto buffer = reader.ReadCode(functionRva, function.length_);
				if (buffer.empty())
					continue;

				// One decode pass: instruction terminators (for the CFG),
				// block leaders, and MSVC counter increments.
				struct Terminator
				{
					std::uint32_t rva_;
					ZydisInstructionCategory category_;
					bool hasTarget_;
					std::uint32_t target_;
				};
				std::vector<Terminator> terminators;
				std::vector<std::uint32_t> leaders;
				leaders.push_back(functionRva);
				std::vector<std::pair<std::uint32_t, std::uint32_t>>
				    increments;
				bool indirect = false;

				std::size_t offset = 0;
				while (offset < buffer.size())
				{
					ZydisDecodedInstruction instruction;
					ZydisDecodedOperand operands[ZYDIS_MAX_OPERAND_COUNT];
					if (!ZYAN_SUCCESS(ZydisDecoderDecodeFull(
					        &decoder,
					        buffer.data() + offset,
					        buffer.size() - offset,
					        &instruction,
					        operands)))
					{
						++offset;
						continue;
					}

					auto instructionRva = functionRva +
					                      static_cast<std::uint32_t>(offset);
					auto nextRva = instructionRva + instruction.length;
					auto category = instruction.meta.category;

					if (category == ZYDIS_CATEGORY_COND_BR ||
					    category == ZYDIS_CATEGORY_UNCOND_BR ||
					    category == ZYDIS_CATEGORY_RET ||
					    category == ZYDIS_CATEGORY_INTERRUPT)
					{
						// The fall-through after a terminator is a leader only
						// while it is still inside this function; a terminator at
						// the very end (e.g. a trailing ret) would otherwise plant
						// a phantom zero-count block at functionEnd that collides
						// with the next abutting function's first block.
						if (nextRva < functionEnd)
							leaders.push_back(nextRva);
						Terminator terminator{instructionRva, category, false,
						                      0};
						if (category == ZYDIS_CATEGORY_COND_BR ||
						    category == ZYDIS_CATEGORY_UNCOND_BR)
						{
							ZyanU64 target = 0;
							if (operands[0].type ==
							        ZYDIS_OPERAND_TYPE_IMMEDIATE &&
							    ZYAN_SUCCESS(ZydisCalcAbsoluteAddress(
							        &instruction, &operands[0],
							        instructionRva, &target)))
							{
								if (target >= functionRva &&
								    target < functionEnd)
								{
									terminator.hasTarget_ = true;
									terminator.target_ =
									    static_cast<std::uint32_t>(target);
									leaders.push_back(terminator.target_);
								}
								// A direct target outside the function is a
								// tail call: no in-function successor.
							}
							else
							{
								// jmp reg / jmp [mem]: unknown targets.
								indirect = true;
							}
						}
						terminators.push_back(terminator);
					}

					// MSVC's per-block counters are `inc byte [rip+disp]` (some
					// builds `add byte [rip+disp], 1`). Only those instructions
					// write the .sancov region, so gate on the mnemonic: a stray
					// lea/read into the section would otherwise be mistaken for a
					// counter.
					if (region &&
					    (instruction.mnemonic == ZYDIS_MNEMONIC_INC ||
					     instruction.mnemonic == ZYDIS_MNEMONIC_ADD))
					{
						for (int i = 0; i < instruction.operand_count; ++i)
						{
							const auto& operand = operands[i];
							if (operand.type != ZYDIS_OPERAND_TYPE_MEMORY ||
							    operand.mem.base != ZYDIS_REGISTER_RIP)
								continue;
							ZyanU64 target = 0;
							if (!ZYAN_SUCCESS(ZydisCalcAbsoluteAddress(
							        &instruction, &operand, instructionRva,
							        &target)))
								continue;
							if (target < regionRva || target >= regionEnd)
								continue;
							increments.emplace_back(
							    instructionRva,
							    (*region)[static_cast<std::size_t>(
							        target - regionRva)]);
						}
					}

					offset += instruction.length;
				}

				std::sort(leaders.begin(), leaders.end());
				leaders.erase(std::unique(leaders.begin(), leaders.end()),
				              leaders.end());
				auto blockCount = leaders.size();

				auto leaderIndex = [&](std::uint32_t rva) -> int {
					auto it = std::upper_bound(
					    leaders.begin(), leaders.end(), rva);
					if (it == leaders.begin())
						return -1;
					return static_cast<int>((it - 1) - leaders.begin());
				};

				// Successors of each block, from its terminating instruction.
				std::vector<std::vector<int>> successors(blockCount);
				std::vector<std::vector<int>> predecessors(blockCount);
				std::size_t nextTerminator = 0;
				for (std::size_t b = 0; b < blockCount; ++b)
				{
					auto blockEnd = (b + 1 < blockCount) ? leaders[b + 1]
					                                     : functionEnd;
					// The terminator of block b is the last control-transfer
					// instruction inside it, if any.
					const Terminator* terminator = nullptr;
					while (nextTerminator < terminators.size() &&
					       terminators[nextTerminator].rva_ < blockEnd)
						terminator = &terminators[nextTerminator++];

					if (!terminator)
					{
						// Falls straight into the next block.
						if (b + 1 < blockCount)
							successors[b].push_back(
							    static_cast<int>(b + 1));
						continue;
					}
					switch (terminator->category_)
					{
						case ZYDIS_CATEGORY_RET:
						case ZYDIS_CATEGORY_INTERRUPT:
							break; // exit
						case ZYDIS_CATEGORY_UNCOND_BR:
							if (terminator->hasTarget_)
							{
								auto t = leaderIndex(terminator->target_);
								if (t >= 0)
									successors[b].push_back(t);
							}
							break;
						case ZYDIS_CATEGORY_COND_BR:
							if (terminator->hasTarget_)
							{
								auto t = leaderIndex(terminator->target_);
								if (t >= 0)
									successors[b].push_back(t);
							}
							if (b + 1 < blockCount)
								successors[b].push_back(
								    static_cast<int>(b + 1));
							break;
						default:
							if (b + 1 < blockCount)
								successors[b].push_back(
								    static_cast<int>(b + 1));
							break;
					}
				}
				for (std::size_t b = 0; b < blockCount; ++b)
					for (auto s : successors[b])
						predecessors[s].push_back(static_cast<int>(b));

				// Known counts per block (-1 = not instrumented).
				std::vector<int> count(blockCount, -1);
				auto setCount = [&](std::uint32_t rva, std::uint32_t value) {
					auto index = leaderIndex(rva);
					if (index < 0)
						return;
					count[index] = std::max<int>(
					    count[index], static_cast<int>(value));
				};
				for (const auto& [incRva, value] : increments)
					setCount(incRva, value);
				if (clangCounts)
				{
					for (auto it = clangCounts->lower_bound(functionRva);
					     it != clangCounts->end() &&
					     it->first < functionEnd;
					     ++it)
						setCount(it->first, it->second);
				}

				std::vector<char> covered(blockCount, 0);
				bool anyCovered = false;
				for (std::size_t b = 0; b < blockCount; ++b)
				{
					if (count[b] > 0)
					{
						covered[b] = 1;
						anyCovered = true;
					}
				}

				if (anyCovered)
					covered[0] = 1; // the entry dominates every block

				if (!indirect)
				{
					bool changed = true;
					while (changed)
					{
						changed = false;
						for (std::size_t b = 0; b < blockCount; ++b)
						{
							if (covered[b] && successors[b].size() == 1 &&
							    !covered[successors[b][0]])
							{
								covered[successors[b][0]] = 1;
								changed = true;
							}
							if (covered[b] &&
							    predecessors[b].size() == 1 &&
							    !covered[predecessors[b][0]])
							{
								covered[predecessors[b][0]] = 1;
								changed = true;
							}
						}
					}
				}

				for (std::size_t b = 0; b < blockCount; ++b)
				{
					std::uint32_t value =
					    count[b] > 0 ? static_cast<std::uint32_t>(count[b])
					                 : (covered[b] ? 1u : 0u);
					blocks.push_back(BasicBlock{leaders[b], value});
				}
			}

			return blocks;
		}

		//---------------------------------------------------------------------
		// Resolved dump (clang): the runtime already resolved each basic block
		// to (module, rva, count). Group by module and hand the blocks over.
		template <typename ProcessFn>
		void ImportResolvedDump(const std::vector<char>& bytes,
		                        const std::filesystem::path& dumpPath,
		                        ProcessFn&& processModule)
		{
			if (bytes.size() < sizeof(FastCov::DumpHeader))
				THROW(L"Fast coverage dump is truncated: " +
				      dumpPath.wstring());
			const auto* header =
			    reinterpret_cast<const FastCov::DumpHeader*>(bytes.data());
			auto expected =
			    sizeof(FastCov::DumpHeader) +
			    static_cast<std::size_t>(header->moduleCount_) *
			        sizeof(FastCov::ModuleEntry) +
			    static_cast<std::size_t>(header->basicBlockCount_) *
			        sizeof(FastCov::BasicBlockRecord);
			if (header->version_ != FastCov::DumpVersion ||
			    bytes.size() < expected)
				THROW(L"Fast coverage dump is truncated: " +
				      dumpPath.wstring());

			const auto* modules =
			    reinterpret_cast<const FastCov::ModuleEntry*>(
			        bytes.data() + sizeof(FastCov::DumpHeader));
			const auto* records =
			    reinterpret_cast<const FastCov::BasicBlockRecord*>(
			        bytes.data() + sizeof(FastCov::DumpHeader) +
			        static_cast<std::size_t>(header->moduleCount_) *
			            sizeof(FastCov::ModuleEntry));

			// The clang PC table already gives each instrumented block's
			// address; collect the counts per module so the shared
			// reconstruction can rebuild the CFG and fill the pruned blocks.
			std::vector<std::map<std::uint32_t, std::uint32_t>> countsByModule(
			    header->moduleCount_);
			for (std::uint32_t i = 0; i < header->basicBlockCount_; ++i)
			{
				const auto& record = records[i];
				if (record.moduleIndex_ < header->moduleCount_)
				{
					auto& counts = countsByModule[record.moduleIndex_];
					auto& value = counts[record.rva_];
					value = std::max<std::uint32_t>(value, record.count_);
				}
			}

			for (std::uint32_t m = 0; m < header->moduleCount_; ++m)
			{
				if (countsByModule[m].empty())
					continue;
				processModule(std::filesystem::path{modules[m].path_},
				              &countsByModule[m],
				              std::uint32_t{0},
				              nullptr);
			}
		}

		//---------------------------------------------------------------------
		// Raw dump (MSVC): a copy of each module's .sancov counter region. The
		// block addresses are recovered by disassembly in processModule.
		template <typename ProcessFn>
		void ImportRawDump(const std::vector<char>& bytes,
		                   const std::filesystem::path& dumpPath,
		                   ProcessFn&& processModule)
		{
			if (bytes.size() < sizeof(FastCov::RawDumpHeader))
				THROW(L"Fast coverage dump is truncated: " +
				      dumpPath.wstring());
			const auto* header =
			    reinterpret_cast<const FastCov::RawDumpHeader*>(bytes.data());
			if (header->version_ != FastCov::DumpVersion)
				THROW(L"Unsupported fast coverage dump version: " +
				      dumpPath.wstring());

			std::size_t offset = sizeof(FastCov::RawDumpHeader);
			for (std::uint32_t m = 0; m < header->moduleCount_; ++m)
			{
				if (offset + sizeof(FastCov::RawModuleHeader) > bytes.size())
					THROW(L"Fast coverage dump is truncated: " +
					      dumpPath.wstring());
				const auto* moduleHeader =
				    reinterpret_cast<const FastCov::RawModuleHeader*>(
				        bytes.data() + offset);
				offset += sizeof(FastCov::RawModuleHeader);

				if (offset + moduleHeader->regionSize_ > bytes.size())
					THROW(L"Fast coverage dump is truncated: " +
					      dumpPath.wstring());
				const auto* regionBytes =
				    reinterpret_cast<const std::uint8_t*>(bytes.data() +
				                                          offset);
				std::vector<std::uint8_t> region(
				    regionBytes, regionBytes + moduleHeader->regionSize_);
				offset += moduleHeader->regionSize_;

				processModule(std::filesystem::path{moduleHeader->path_},
				              nullptr,
				              moduleHeader->regionRva_,
				              &region);
			}
		}
	}

	//-------------------------------------------------------------------------
	FastCoverageImporter::FastCoverageImporter(
	    const CoverageFilterSettings& coverageFilterSettings,
	    const std::vector<UnifiedDiffSettings>& unifiedDiffSettings,
	    const std::vector<std::wstring>& excludedLineRegexes,
	    const std::vector<SubstitutePdbSourcePath>& substitutePdbSourcePaths)
	    : coverageFilterSettings_{coverageFilterSettings},
	      unifiedDiffSettings_{unifiedDiffSettings},
	      excludedLineRegexes_{excludedLineRegexes},
	      substitutePdbSourcePaths_{substitutePdbSourcePaths}
	{
	}

	//-------------------------------------------------------------------------
	FastCoverageImporter::Result
	FastCoverageImporter::Import(const std::filesystem::path& dumpPath)
	{
		auto bytes = ReadFile(dumpPath);
		if (bytes.size() < sizeof(std::uint32_t))
			THROW(L"Fast coverage dump is truncated: " + dumpPath.wstring());

		auto magic = *reinterpret_cast<const std::uint32_t*>(bytes.data());
		if (magic != FastCov::DumpMagic && magic != FastCov::RawDumpMagic)
			THROW(L"Not a fast coverage dump: " + dumpPath.wstring());

		CoverageFilterManager filterManager{coverageFilterSettings_,
		                                     unifiedDiffSettings_,
		                                     excludedLineRegexes_,
		                                     /*useReleaseCoverageFilter*/ false};

		ExecutedAddressManager manager;
		std::wstring coverageName;
		std::uintptr_t nextTag = 1;

		// stem -> instantiation name -> (file, line) -> count.
		std::map<std::wstring,
		         std::map<std::wstring,
		                  std::pair<std::filesystem::path,
		                            std::map<std::uint32_t, std::uint32_t>>>>
		    templates;

		// Maps one module's coverage to line and instantiation coverage. The
		// instrumented block counts come from the clang PC table (clangCounts)
		// or, for MSVC, from the .sancov region (rawRegion); either way the
		// shared reconstruction rebuilds the CFG and fills the pruned blocks.
		auto processModule =
		    [&](const std::filesystem::path& modulePath,
		        const std::map<std::uint32_t, std::uint32_t>* clangCounts,
		        std::uint32_t regionRva,
		        const std::vector<std::uint8_t>* rawRegion) {
			    if (!filterManager.IsModuleSelected(modulePath.wstring()))
				    return;

			    // A fresh enumeration (no cache) so function names resolve.
			    DebugInformationEnumerator enumerator{
			        substitutePdbSourcePaths_,
			        /*needSymbolIndex*/ false,
			        /*lineTableCache*/ nullptr,
			        /*needFunctionNames*/ true};
			    ModuleCollector collector{filterManager, modulePath};
			    if (!enumerator.Enumerate(modulePath, collector))
			    {
				    LOG_WARNING("Fast coverage: no debug information for {}", modulePath.wstring());
				    return;
			    }

			    auto reader = PeImageFileReader::TryCreate(modulePath);
			    if (!reader)
			    {
				    LOG_WARNING("Fast coverage: cannot read module {}", modulePath.wstring());
				    return;
			    }
			    auto blocks = ReconstructCoverage(*reader,
			                                      collector.GetFunctions(),
			                                      clangCounts,
			                                      regionRva,
			                                      rawRegion);
			    if (blocks.empty())
				    return;
			    std::sort(blocks.begin(),
			              blocks.end(),
			              [](const auto& lhs, const auto& rhs) {
				              return lhs.rva_ < rhs.rva_;
			              });

			    if (coverageName.empty())
				    coverageName = modulePath.filename().wstring();

			    auto moduleTag = reinterpret_cast<HANDLE>(nextTag++);
			    manager.AddModule(modulePath.wstring(), nullptr);

			    for (const auto& record : collector.GetLineRecords())
			    {
				    auto count = CountAt(blocks, record.rva_);

				    Address address{
				        moduleTag,
				        reinterpret_cast<void*>(
				            static_cast<std::uintptr_t>(record.rva_))};
				    manager.RegisterAddress(
				        address, record.filePath_, record.lineNumber_, 0);
				    if (count > 0)
					    manager.MarkAddressAsExecuted(address);

				    // Attribute the line to its instantiation.
				    const auto* functionName = FunctionNameAt(
				        collector.GetFunctions(), record.rva_);
				    if (functionName && !functionName->empty())
				    {
					    auto stem = TemplateStem(*functionName);
					    if (!stem.empty())
					    {
						    auto& instantiation =
						        templates[stem][*functionName];
						    instantiation.first = record.filePath_;
						    auto& lineCount =
						        instantiation.second[record.lineNumber_];
						    lineCount = std::max(lineCount, count);
					    }
				    }
			    }
		    };

		if (magic == FastCov::DumpMagic)
			ImportResolvedDump(bytes, dumpPath, processModule);
		else
			ImportRawDump(bytes, dumpPath, processModule);

		Result result{manager.CreateCoverageData(coverageName, 0), {}};

		for (const auto& [stem, instantiations] : templates)
		{
			TemplateReport report;
			report.stem_ = stem;
			for (const auto& [name, fileAndLines] : instantiations)
			{
				Instantiation instantiation;
				instantiation.name_ = name;
				instantiation.file_ = fileAndLines.first;
				for (const auto& [lineNumber, count] : fileAndLines.second)
					instantiation.lines_.push_back(
					    InstantiationLine{lineNumber, count});
				report.instantiations_.push_back(std::move(instantiation));
			}
			result.templates_.push_back(std::move(report));
		}

		return result;
	}

	//-------------------------------------------------------------------------
	std::wstring FastCoverageImporter::FormatTemplateReport(
	    const std::vector<TemplateReport>& templates)
	{
		std::wostringstream ostr;
		ostr << L"# Template instantiation coverage\n\n";
		if (templates.empty())
		{
			ostr << L"No template instantiations found.\n";
			return ostr.str();
		}

		for (const auto& report : templates)
		{
			ostr << L"## " << report.stem_ << L"\n\n";

			// The union of lines across instantiations, so the table shows
			// which lines ran for which instantiation.
			std::map<std::uint32_t, bool> allLines;
			for (const auto& instantiation : report.instantiations_)
				for (const auto& line : instantiation.lines_)
					allLines[line.lineNumber_];

			for (const auto& instantiation : report.instantiations_)
			{
				std::map<std::uint32_t, std::uint32_t> byLine;
				for (const auto& line : instantiation.lines_)
					byLine[line.lineNumber_] = line.count_;

				std::size_t covered = 0;
				for (const auto& [lineNumber, count] : byLine)
					if (count > 0)
						++covered;

				ostr << L"- `" << instantiation.name_ << L"` — "
				     << covered << L"/" << byLine.size() << L" lines"
				     << L" (" << instantiation.file_.filename().wstring()
				     << L")\n";
			}

			// Lines covered by some but not all instantiations are the gaps a
			// plain per-line boolean hides.
			ostr << L"\n  Lines covered in some but not all instantiations:";
			bool anyGap = false;
			for (const auto& [lineNumber, unused] : allLines)
			{
				std::size_t coveredIn = 0;
				std::size_t presentIn = 0;
				for (const auto& instantiation : report.instantiations_)
				{
					for (const auto& line : instantiation.lines_)
					{
						if (line.lineNumber_ != lineNumber)
							continue;
						++presentIn;
						if (line.count_ > 0)
							++coveredIn;
					}
				}
				if (presentIn > 0 && coveredIn > 0 && coveredIn < presentIn)
				{
					ostr << L" " << lineNumber;
					anyGap = true;
				}
			}
			ostr << (anyGap ? L"\n\n" : L" (none)\n\n");
		}

		return ostr.str();
	}
}
