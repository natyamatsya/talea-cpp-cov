// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 natyamatsya and talea-cpp-cov contributors

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
#include <zydis/Zydis.h>

#include "tools/Log.hpp"
#include "tools/Tool.hpp"

#include "Address.hpp"
#include "TaleaCppCovException.hpp"
#include "CoverageFilterManager.hpp"
#include "CoverageFilterSettings.hpp"
#include "DebugInformationEnumerator.hpp"
#include "ExecutedAddressManager.hpp"
#include "PeImageFileReader.hpp"

#include "file-filter/ModuleInfo.hpp"
#include "file-filter/FileInfo.hpp"
#include "file-filter/LineInfo.hpp"

#include "coverage/CoverageData.hpp"
#include "coverage/ModuleCoverage.hpp"
#include "coverage/FileCoverage.hpp"
#include "coverage/BranchCoverage.hpp"

#include "coverage-runtime/FastCovFormat.hpp"

namespace TaleaCppCov
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
		// One conditional branch's edge coverage, derived from block coverage:
		// a direction is "covered" when its successor block ran. This is exact
		// when each successor is reached only via this edge (the common case);
		// at a control-flow merge - a successor reached from several edges - it
		// can over-report, the price of deriving branch coverage from block
		// counters rather than observing each Jcc (as the debugger/VEH engine
		// does). Fast-mode branch coverage trades that edge-case exactness for
		// running at native speed.
		struct BranchEdge
		{
			std::uint32_t jccRva_;
			bool taken_;
			bool fallThrough_;
		};

		//---------------------------------------------------------------------
		// A branch's edge coverage placed at its source (module, file, line)
		// with a per-line index, ready to decorate the CoverageData.
		struct BranchResult
		{
			std::wstring modulePath_;
			std::wstring filePath_;
			std::uint32_t lineNumber_;
			std::uint32_t branchIndex_;
			bool taken_;
			bool fallThrough_;
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
		// not a template. "foo<std::int32_t>" -> "foo"; "ns::v<std::int32_t>::at" -> "ns::v".
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
		// Counter-INDEPENDENT control flow of one function: block leaders, the
		// successor/predecessor graph, the conditional-branch structure and the
		// MSVC counter sites (rva + offset into the .sancov region). Reusable
		// across every dump of the module - only the counter VALUES change.
		struct FunctionCfg
		{
			std::uint32_t functionRva_ = 0;
			std::uint32_t functionEnd_ = 0;
			std::vector<std::uint32_t> leaders_;
			std::vector<std::vector<std::int32_t>> successors_;
			std::vector<std::vector<std::int32_t>> predecessors_;
			std::vector<std::uint32_t> condBrJcc_;
			std::vector<std::int32_t> condBrTaken_;
			std::vector<std::int32_t> condBrFall_;
			bool indirect_ = false;
			std::vector<std::pair<std::uint32_t, std::uint32_t>>
			    rawCounterSites_;
		};

		//---------------------------------------------------------------------
		// Everything reconstructed once per module and reused for every dump.
		struct ModuleReconstruction
		{
			std::vector<LineRecord> lineRecords_;
			std::vector<IDebugInformationHandler::FunctionRange> functions_;
			std::vector<FunctionCfg> functionCfgs_;
		};

		//---------------------------------------------------------------------
		// Disassemble one function into its counter-independent CFG. `region`,
		// when present (MSVC raw dumps), is used only for its bounds; the counter
		// VALUES are read per dump in ApplyFunctionCoverage, so the recorded
		// sites (instruction rva, region offset) are valid across every dump.
		FunctionCfg BuildFunctionCfg(
		    ZydisDecoder& decoder,
		    PeImageFileReader& reader,
		    const IDebugInformationHandler::FunctionRange& function,
		    std::uint32_t regionRva,
		    std::uint64_t regionEnd,
		    bool hasRegion)
		{
			FunctionCfg cfg;
			auto functionRva =
			    static_cast<std::uint32_t>(function.virtualAddress_);
			auto functionEnd = functionRva + function.length_;
			cfg.functionRva_ = functionRva;
			cfg.functionEnd_ = functionEnd;
			auto buffer = reader.ReadCode(functionRva, function.length_);
			if (buffer.empty())
				return cfg;

			// One decode pass: instruction terminators (for the CFG), block
			// leaders, and MSVC counter sites.
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

				auto instructionRva =
				    functionRva + static_cast<std::uint32_t>(offset);
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
					Terminator terminator{instructionRva, category, false, 0};
					if (category == ZYDIS_CATEGORY_COND_BR ||
					    category == ZYDIS_CATEGORY_UNCOND_BR)
					{
						ZyanU64 target = 0;
						if (operands[0].type ==
						        ZYDIS_OPERAND_TYPE_IMMEDIATE &&
						    ZYAN_SUCCESS(ZydisCalcAbsoluteAddress(
						        &instruction, &operands[0], instructionRva,
						        &target)))
						{
							if (target >= functionRva && target < functionEnd)
							{
								terminator.hasTarget_ = true;
								terminator.target_ =
								    static_cast<std::uint32_t>(target);
								leaders.push_back(terminator.target_);
							}
							// A direct target outside the function is a tail
							// call: no in-function successor.
						}
						else
						{
							// jmp reg / jmp [mem]: unknown targets.
							indirect = true;
						}
					}
					terminators.push_back(terminator);
				}

				// A per-block counter is any instruction that WRITES the .sancov
				// region: `inc byte [rip+disp]`, `add byte [rip+disp], 1`, and -
				// when /O2 hoists a loop's counter into a register and stores it
				// back after the loop - `mov byte [rip+disp], reg`. Gating on a
				// written memory operand (not the mnemonic) catches all three
				// while excluding a read (`movzx reg, byte [rip+disp]`) or an
				// `lea`. Record the site (rva + region offset); the value is read
				// per dump.
				if (hasRegion)
				{
					for (std::int32_t i = 0; i < instruction.operand_count; ++i)
					{
						const auto& operand = operands[i];
						if (operand.type != ZYDIS_OPERAND_TYPE_MEMORY ||
						    operand.mem.base != ZYDIS_REGISTER_RIP)
							continue;
						if (!(operand.actions &
						      (ZYDIS_OPERAND_ACTION_WRITE |
						       ZYDIS_OPERAND_ACTION_CONDWRITE)))
							continue; // a read/lea into the section, not a counter
						ZyanU64 target = 0;
						if (!ZYAN_SUCCESS(ZydisCalcAbsoluteAddress(
						        &instruction, &operand, instructionRva,
						        &target)))
							continue;
						if (target < regionRva || target >= regionEnd)
							continue;
						cfg.rawCounterSites_.emplace_back(
						    instructionRva,
						    static_cast<std::uint32_t>(target - regionRva));
					}
				}

				offset += instruction.length;
			}

			std::sort(leaders.begin(), leaders.end());
			leaders.erase(std::unique(leaders.begin(), leaders.end()),
			              leaders.end());
			auto blockCount = leaders.size();

			auto leaderIndex = [&](std::uint32_t rva) -> std::int32_t {
				auto it =
				    std::upper_bound(leaders.begin(), leaders.end(), rva);
				if (it == leaders.begin())
					return -1;
				return static_cast<std::int32_t>((it - 1) - leaders.begin());
			};

			// Successors of each block, from its terminating instruction.
			std::vector<std::vector<std::int32_t>> successors(blockCount);
			std::vector<std::vector<std::int32_t>> predecessors(blockCount);
			std::vector<std::uint32_t> condBrJcc(blockCount, 0);
			std::vector<std::int32_t> condBrTaken(blockCount, -1);
			std::vector<std::int32_t> condBrFall(blockCount, -1);
			std::size_t nextTerminator = 0;
			for (std::size_t b = 0; b < blockCount; ++b)
			{
				auto blockEnd =
				    (b + 1 < blockCount) ? leaders[b + 1] : functionEnd;
				const Terminator* terminator = nullptr;
				while (nextTerminator < terminators.size() &&
				       terminators[nextTerminator].rva_ < blockEnd)
					terminator = &terminators[nextTerminator++];

				if (!terminator)
				{
					if (b + 1 < blockCount)
						successors[b].push_back(
						    static_cast<std::int32_t>(b + 1));
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
					{
						std::int32_t takenBlock = -1;
						if (terminator->hasTarget_)
						{
							takenBlock = leaderIndex(terminator->target_);
							if (takenBlock >= 0)
								successors[b].push_back(takenBlock);
						}
						std::int32_t fallBlock =
						    (b + 1 < blockCount)
						        ? static_cast<std::int32_t>(b + 1)
						        : -1;
						if (fallBlock >= 0)
							successors[b].push_back(fallBlock);
						if (takenBlock >= 0 && fallBlock >= 0)
						{
							condBrJcc[b] = terminator->rva_;
							condBrTaken[b] = takenBlock;
							condBrFall[b] = fallBlock;
						}
						break;
					}
					default:
						if (b + 1 < blockCount)
							successors[b].push_back(
							    static_cast<std::int32_t>(b + 1));
						break;
				}
			}
			for (std::size_t b = 0; b < blockCount; ++b)
				for (auto s : successors[b])
					predecessors[s].push_back(static_cast<std::int32_t>(b));

			cfg.leaders_ = std::move(leaders);
			cfg.successors_ = std::move(successors);
			cfg.predecessors_ = std::move(predecessors);
			cfg.condBrJcc_ = std::move(condBrJcc);
			cfg.condBrTaken_ = std::move(condBrTaken);
			cfg.condBrFall_ = std::move(condBrFall);
			cfg.indirect_ = indirect;
			return cfg;
		}

		//---------------------------------------------------------------------
		// Apply one dump's counters to a cached function CFG, appending its
		// basic blocks (and, when requested, branch edges). This is the only
		// per-dump work; the disassembly above is done once per module.
		void ApplyFunctionCoverage(
		    const FunctionCfg& cfg,
		    const std::map<std::uint32_t, std::uint32_t>* clangCounts,
		    const std::vector<std::uint8_t>* region,
		    std::vector<BasicBlock>& blocks,
		    std::vector<BranchEdge>* branchEdges)
		{
			auto blockCount = cfg.leaders_.size();
			if (blockCount == 0)
				return;

			auto leaderIndex = [&](std::uint32_t rva) -> std::int32_t {
				auto it = std::upper_bound(
				    cfg.leaders_.begin(), cfg.leaders_.end(), rva);
				if (it == cfg.leaders_.begin())
					return -1;
				return static_cast<std::int32_t>(
				    (it - 1) - cfg.leaders_.begin());
			};

			// Known counts per block (-1 = not instrumented).
			std::vector<std::int32_t> count(blockCount, -1);
			auto setCount = [&](std::uint32_t rva, std::uint32_t value) {
				auto index = leaderIndex(rva);
				if (index < 0)
					return;
				count[index] = std::max<std::int32_t>(
				    count[index], static_cast<std::int32_t>(value));
			};
			if (region)
			{
				for (const auto& [incRva, regionOffset] : cfg.rawCounterSites_)
					if (regionOffset < region->size())
						setCount(incRva, (*region)[regionOffset]);
			}
			if (clangCounts)
			{
				for (auto it = clangCounts->lower_bound(cfg.functionRva_);
				     it != clangCounts->end() && it->first < cfg.functionEnd_;
				     ++it)
					setCount(it->first, it->second);
			}

			std::vector<char> covered(blockCount, 0);
			bool anyCovered = false;
			for (std::size_t b = 0; b < blockCount; ++b)
				if (count[b] > 0)
				{
					covered[b] = 1;
					anyCovered = true;
				}

			if (anyCovered)
				covered[0] = 1; // the entry dominates every block

			if (!cfg.indirect_)
			{
				bool changed = true;
				while (changed)
				{
					changed = false;
					for (std::size_t b = 0; b < blockCount; ++b)
					{
						if (covered[b] && cfg.successors_[b].size() == 1 &&
						    !covered[cfg.successors_[b][0]])
						{
							covered[cfg.successors_[b][0]] = 1;
							changed = true;
						}
						if (covered[b] && cfg.predecessors_[b].size() == 1 &&
						    !covered[cfg.predecessors_[b][0]])
						{
							covered[cfg.predecessors_[b][0]] = 1;
							changed = true;
						}
					}
				}
			}

			// Branch-edge coverage: each conditional block's two edges are
			// covered iff their successor blocks ran (see BranchEdge).
			if (branchEdges)
			{
				for (std::size_t b = 0; b < blockCount; ++b)
				{
					if (cfg.condBrJcc_[b] == 0)
						continue;
					branchEdges->push_back(BranchEdge{
					    cfg.condBrJcc_[b],
					    covered[cfg.condBrTaken_[b]] != 0,
					    covered[cfg.condBrFall_[b]] != 0});
				}
			}

			for (std::size_t b = 0; b < blockCount; ++b)
			{
				std::uint32_t value =
				    count[b] > 0 ? static_cast<std::uint32_t>(count[b])
				                 : (covered[b] ? 1u : 0u);
				blocks.push_back(BasicBlock{cfg.leaders_[b], value});
			}
		}

		//---------------------------------------------------------------------
		// Reconstruct one module's per-function CFGs once (the disassembly is
		// counter-independent, so the caller caches the result per module).
		std::vector<FunctionCfg> BuildFunctionCfgs(
		    PeImageFileReader& reader,
		    const std::vector<IDebugInformationHandler::FunctionRange>&
		        functions,
		    std::uint32_t regionRva,
		    const std::vector<std::uint8_t>* region)
		{
			std::vector<FunctionCfg> cfgs;
			ZydisDecoder decoder;
			if (!ZYAN_SUCCESS(ZydisDecoderInit(&decoder,
			                                   ZYDIS_MACHINE_MODE_LONG_64,
			                                   ZYDIS_STACK_WIDTH_64)))
				return cfgs;
			std::uint64_t regionEnd =
			    region ? std::uint64_t{regionRva} + region->size() : 0;
			cfgs.reserve(functions.size());
			for (const auto& function : functions)
				cfgs.push_back(BuildFunctionCfg(decoder,
				                                reader,
				                                function,
				                                regionRva,
				                                regionEnd,
				                                region != nullptr));
			return cfgs;
		}

		//---------------------------------------------------------------------
		// Apply one dump's counters across all of a module's cached function
		// CFGs. Blocks are returned unsorted; the caller sorts by rva.
		std::vector<BasicBlock> ApplyModuleCoverage(
		    const std::vector<FunctionCfg>& cfgs,
		    const std::map<std::uint32_t, std::uint32_t>* clangCounts,
		    const std::vector<std::uint8_t>* region,
		    std::vector<BranchEdge>* branchEdges)
		{
			std::vector<BasicBlock> blocks;
			for (const auto& cfg : cfgs)
				ApplyFunctionCoverage(
				    cfg, clangCounts, region, blocks, branchEdges);
			return blocks;
		}

		//---------------------------------------------------------------------
		// Resolved dump (clang): the runtime already resolved each basic block
		// to (module, rva, count). Group by module and hand the blocks over.
		template <typename ProcessFn>
		Tools::Expected<void> ImportResolvedDump(const std::vector<char>& bytes,
		                        const std::filesystem::path& dumpPath,
		                        ProcessFn&& processModule)
		{
			if (bytes.size() < sizeof(FastCov::DumpHeader))
				return std::unexpected(MAKE_ERROR(
				    L"Fast coverage dump is truncated: " + dumpPath.wstring()));
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
				return std::unexpected(MAKE_ERROR(
				    L"Fast coverage dump is truncated: " + dumpPath.wstring()));

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
			return {};
		}

		//---------------------------------------------------------------------
		// Raw dump (MSVC): a copy of each module's .sancov counter region. The
		// block addresses are recovered by disassembly in processModule.
		// Version 1 stores each module path in a fixed-size field, version 2
		// stores it with its length (see FastCovFormat.hpp).
		template <typename ProcessFn>
		Tools::Expected<void> ImportRawDump(const std::vector<char>& bytes,
		                   const std::filesystem::path& dumpPath,
		                   ProcessFn&& processModule)
		{
			if (bytes.size() < sizeof(FastCov::RawDumpHeader))
				return std::unexpected(MAKE_ERROR(
				    L"Fast coverage dump is truncated: " + dumpPath.wstring()));
			const auto* header =
			    reinterpret_cast<const FastCov::RawDumpHeader*>(bytes.data());
			const bool version2 = header->version_ == FastCov::RawDumpVersion2;
			if (!version2 && header->version_ != FastCov::DumpVersion)
				return std::unexpected(MAKE_ERROR(
				    L"Unsupported fast coverage dump version: " +
				    dumpPath.wstring()));

			auto truncated = [&]() {
				return std::unexpected(MAKE_ERROR(
				    L"Fast coverage dump is truncated: " + dumpPath.wstring()));
			};

			std::size_t offset = sizeof(FastCov::RawDumpHeader);
			for (std::uint32_t m = 0; m < header->moduleCount_; ++m)
			{
				std::wstring modulePath;
				std::uint32_t regionRva = 0;
				std::uint32_t regionSize = 0;
				if (version2)
				{
					if (offset + sizeof(FastCov::RawModuleHeaderV2) > bytes.size())
						return truncated();
					const auto* moduleHeader =
					    reinterpret_cast<const FastCov::RawModuleHeaderV2*>(
					        bytes.data() + offset);
					offset += sizeof(FastCov::RawModuleHeaderV2);

					auto pathBytes = static_cast<std::size_t>(
					                     moduleHeader->pathChars_) *
					                 sizeof(wchar_t);
					if (offset + pathBytes > bytes.size())
						return truncated();
					modulePath.resize(moduleHeader->pathChars_);
					std::memcpy(
					    modulePath.data(), bytes.data() + offset, pathBytes);
					offset += pathBytes;
					regionRva = moduleHeader->regionRva_;
					regionSize = moduleHeader->regionSize_;
				}
				else
				{
					if (offset + sizeof(FastCov::RawModuleHeader) > bytes.size())
						return truncated();
					const auto* moduleHeader =
					    reinterpret_cast<const FastCov::RawModuleHeader*>(
					        bytes.data() + offset);
					offset += sizeof(FastCov::RawModuleHeader);

					modulePath.assign(moduleHeader->path_,
					                  wcsnlen(moduleHeader->path_,
					                          FastCov::MaxModulePathChars));
					regionRva = moduleHeader->regionRva_;
					regionSize = moduleHeader->regionSize_;
				}

				if (offset + regionSize > bytes.size())
					return truncated();
				const auto* regionBytes =
				    reinterpret_cast<const std::uint8_t*>(bytes.data() +
				                                          offset);
				std::vector<std::uint8_t> region(regionBytes,
				                                 regionBytes + regionSize);
				offset += regionSize;

				processModule(std::filesystem::path{modulePath},
				              nullptr,
				              regionRva,
				              &region);
			}
			return {};
		}
	}

	// Memoized per-module reconstruction, keyed by module path, reused across
	// every Import() call on this importer.
	struct FastCoverageImporter::ModuleCache
	{
		std::unordered_map<std::wstring, ModuleReconstruction> map_;
	};

	//-------------------------------------------------------------------------
	FastCoverageImporter::FastCoverageImporter(
	    const CoverageFilterSettings& coverageFilterSettings,
	    const std::vector<UnifiedDiffSettings>& unifiedDiffSettings,
	    const std::vector<std::wstring>& excludedLineRegexes,
	    const std::vector<SubstitutePdbSourcePath>& substitutePdbSourcePaths)
	    : coverageFilterSettings_{coverageFilterSettings},
	      unifiedDiffSettings_{unifiedDiffSettings},
	      excludedLineRegexes_{excludedLineRegexes},
	      substitutePdbSourcePaths_{substitutePdbSourcePaths},
	      moduleCache_{std::make_unique<ModuleCache>()}
	{
	}

	//-------------------------------------------------------------------------
	FastCoverageImporter::~FastCoverageImporter() = default;

	//-------------------------------------------------------------------------
	Tools::Expected<FastCoverageImporter::Result>
	FastCoverageImporter::Import(const std::filesystem::path& dumpPath)
	{
		try
		{
			return ImportOrThrow(dumpPath);
		}
		catch (const std::exception& e)
		{
			return std::unexpected(Tools::ErrorFromException(e));
		}
	}

	//-------------------------------------------------------------------------
	// Format errors are returned by value; the per-module work (Enumerate, the
	// ExecutedAddressManager) can still throw and is caught by Import() until
	// those leaves are no-throw too.
	Tools::Expected<FastCoverageImporter::Result>
	FastCoverageImporter::ImportOrThrow(const std::filesystem::path& dumpPath)
	{
		auto bytes = ReadFile(dumpPath);
		if (bytes.size() < sizeof(std::uint32_t))
			return std::unexpected(MAKE_ERROR(
			    L"Fast coverage dump is truncated: " + dumpPath.wstring()));

		auto magic = *reinterpret_cast<const std::uint32_t*>(bytes.data());
		if (magic != FastCov::DumpMagic && magic != FastCov::RawDumpMagic)
			return std::unexpected(
			    MAKE_ERROR(L"Not a fast coverage dump: " + dumpPath.wstring()));

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

		// Branch-edge coverage placed at source, collected across modules and
		// attached to the CoverageData after it is built.
		std::vector<BranchResult> branchResults;

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

			    // Reconstruct the module (PDB records + per-function CFG) once
			    // and memoize it: a suite's many dumps of the same module each
			    // cost only a counter-apply, not a re-parse and re-disassemble.
			    auto cacheIt = moduleCache_->map_.find(modulePath.wstring());
			    if (cacheIt == moduleCache_->map_.end())
			    {
				    // A fresh enumeration (no cache) so function names resolve.
				    DebugInformationEnumerator enumerator{
				        substitutePdbSourcePaths_,
				        /*needSymbolIndex*/ false,
				        /*lineTableCache*/ nullptr,
				        /*needFunctionNames*/ true};
				    ModuleCollector collector{filterManager, modulePath};
				    if (!Tools::ValueOrThrow(
				            enumerator.Enumerate(modulePath, collector)))
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
				    ModuleReconstruction recon;
				    recon.lineRecords_ = std::move(collector.GetLineRecords());
				    recon.functions_ = std::move(collector.GetFunctions());
				    recon.functionCfgs_ = BuildFunctionCfgs(
				        *reader, recon.functions_, regionRva, rawRegion);
				    cacheIt = moduleCache_->map_
				                  .emplace(modulePath.wstring(),
				                           std::move(recon))
				                  .first;
			    }
			    const ModuleReconstruction& recon = cacheIt->second;

			    std::vector<BranchEdge> branchEdges;
			    auto blocks = ApplyModuleCoverage(
			        recon.functionCfgs_, clangCounts, rawRegion, &branchEdges);
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

			    for (const auto& record : recon.lineRecords_)
			    {
				    auto count = CountAt(blocks, record.rva_);

				    Address address{
				        moduleTag,
				        reinterpret_cast<void*>(
				            static_cast<std::uintptr_t>(record.rva_))};
				    // AddModule above sets the last module, so this cannot fail;
				    // bridge the invariant back to a throw (this scope is inside
				    // ImportOrThrow's try/catch, which turns it into unexpected).
				    [[maybe_unused]] auto kept = Tools::ValueOrThrow(
				        manager.RegisterAddress(
				            address, record.filePath_, record.lineNumber_, 0));
				    if (count > 0)
					    manager.MarkAddressAsExecuted(address);

				    // Attribute the line to its instantiation.
				    const auto* functionName = FunctionNameAt(
				        recon.functions_, record.rva_);
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

			    // Place each conditional branch at its source line - the line
			    // record with the greatest rva <= the Jcc's - and give it a
			    // per-(file,line) index, matching how the debugger/VEH engine
			    // numbers the branches of a line. A Jcc is only attributed when
			    // that line record lies in the SAME function: otherwise a Jcc
			    // in an unselected function (CRT, the coverage runtime) would be
			    // mis-charged to the nearest preceding selected line.
			    if (!branchEdges.empty())
			    {
				    auto sortedRecords = recon.lineRecords_;
				    std::sort(sortedRecords.begin(),
				              sortedRecords.end(),
				              [](const LineRecord& lhs, const LineRecord& rhs) {
					              return lhs.rva_ < rhs.rva_;
				              });

				    auto functions = recon.functions_;
				    std::sort(functions.begin(),
				              functions.end(),
				              [](const auto& lhs, const auto& rhs) {
					              return lhs.virtualAddress_ <
					                     rhs.virtualAddress_;
				              });
				    auto enclosingFunctionStart =
				        [&](std::uint32_t rva) -> std::int64_t {
					    auto it = std::upper_bound(
					        functions.begin(),
					        functions.end(),
					        rva,
					        [](std::uint32_t value, const auto& function) {
						        return static_cast<std::int64_t>(value) <
						               function.virtualAddress_;
					        });
					    if (it == functions.begin())
						    return -1;
					    const auto& function = *std::prev(it);
					    if (static_cast<std::int64_t>(rva) >=
					        function.virtualAddress_ +
					            static_cast<std::int64_t>(function.length_))
						    return -1; // between functions
					    return function.virtualAddress_;
				    };

				    std::map<std::pair<std::wstring, std::uint32_t>,
				             std::uint32_t>
				        branchIndexByLine;
				    for (const auto& edge : branchEdges)
				    {
					    auto functionStart =
					        enclosingFunctionStart(edge.jccRva_);
					    if (functionStart < 0)
						    continue;

					    auto it = std::upper_bound(
					        sortedRecords.begin(),
					        sortedRecords.end(),
					        edge.jccRva_,
					        [](std::uint32_t value, const LineRecord& record) {
						        return value < record.rva_;
					        });
					    if (it == sortedRecords.begin())
						    continue; // Jcc before the first selected line
					    const auto& record = *std::prev(it);
					    // The line must belong to the Jcc's own function.
					    if (static_cast<std::int64_t>(record.rva_) <
					        functionStart)
						    continue;

					    auto key = std::make_pair(record.filePath_,
					                              record.lineNumber_);
					    auto branchIndex = branchIndexByLine[key]++;
					    branchResults.push_back(
					        BranchResult{modulePath.wstring(),
					                     record.filePath_,
					                     record.lineNumber_,
					                     branchIndex,
					                     edge.taken_,
					                     edge.fallThrough_});
				    }
			    }
		    };

		if (magic == FastCov::DumpMagic)
		{
			if (auto r = ImportResolvedDump(bytes, dumpPath, processModule); !r)
				return std::unexpected(std::move(r.error()));
		}
		else
		{
			if (auto r = ImportRawDump(bytes, dumpPath, processModule); !r)
				return std::unexpected(std::move(r.error()));
		}

		Result result{manager.CreateCoverageData(coverageName, 0), {}};

		// Attach the branch-edge coverage to the built CoverageData: locate
		// each result's file and add the branch to its line (only when the
		// line survived filtering), mirroring the debugger/VEH decoration.
		if (!branchResults.empty())
		{
			std::unordered_map<
			    std::wstring,
			    std::unordered_map<std::wstring, Coverage::FileCoverage*>>
			    filesByModule;
			for (const auto& module : result.coverageData_.GetModules())
			{
				auto& files = filesByModule[module->GetPath().wstring()];
				for (const auto& file : module->GetFiles())
					files[file->GetPath().wstring()] = file.get();
			}
			for (const auto& branch : branchResults)
			{
				auto moduleIt = filesByModule.find(branch.modulePath_);
				if (moduleIt == filesByModule.end())
					continue;
				auto fileIt = moduleIt->second.find(branch.filePath_);
				if (fileIt == moduleIt->second.end())
					continue;
				auto* file = fileIt->second;
				if (!(*file)[branch.lineNumber_])
					continue; // the line was filtered out; skip its branch
				file->AddBranch(branch.lineNumber_,
				                Coverage::BranchCoverage{branch.branchIndex_,
				                                       branch.taken_,
				                                       branch.fallThrough_});
			}
		}

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
