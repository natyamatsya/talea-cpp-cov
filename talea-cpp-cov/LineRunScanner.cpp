// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 natyamatsya and talea-cpp-cov contributors

#include "stdafx.h"
#include <cstdint>
#include "LineRunScanner.hpp"

#include <algorithm>
#include <unordered_set>

#ifndef ZYDIS_STATIC_BUILD
#define ZYDIS_STATIC_BUILD
#endif
#include <zydis/Zydis.h>

namespace TaleaCppCov
{
	//-------------------------------------------------------------------------
	class LineRunScanner::Impl
	{
	  public:
		ZydisDecoder decoder_;
	};

	//-------------------------------------------------------------------------
	LineRunScanner::LineRunScanner(bool is64BitCode)
	    : impl_{std::make_unique<Impl>()}
	{
		auto status = is64BitCode
		                  ? ZydisDecoderInit(&impl_->decoder_,
		                                     ZYDIS_MACHINE_MODE_LONG_64,
		                                     ZYDIS_STACK_WIDTH_64)
		                  : ZydisDecoderInit(&impl_->decoder_,
		                                     ZYDIS_MACHINE_MODE_LEGACY_32,
		                                     ZYDIS_STACK_WIDTH_32);
		if (!ZYAN_SUCCESS(status))
			impl_ = nullptr;
	}

	//-------------------------------------------------------------------------
	LineRunScanner::~LineRunScanner() = default;

	//-------------------------------------------------------------------------
	std::optional<std::vector<LineRun>>
	LineRunScanner::ComputeRuns(const std::uint8_t* buffer,
	                            size_t size,
	                            DWORD64 startVirtualAddress,
	                            const std::vector<LineRunLine>& lines) const
	{
		if (!impl_ || size == 0)
			return std::nullopt;

		// Pass 1: decode linearly; collect instruction boundaries,
		// run-ending instructions and static branch targets. Any decode
		// failure or indirect jump rejects the whole function.
		std::vector<size_t> instructionOffsets;
		std::vector<bool> endsRun;
		std::unordered_set<DWORD64> targets;

		size_t offset = 0;
		while (offset < size)
		{
			ZydisDecodedInstruction instruction;
			ZydisDecodedOperand operands[ZYDIS_MAX_OPERAND_COUNT];
			auto status = ZydisDecoderDecodeFull(&impl_->decoder_,
			                                     buffer + offset,
			                                     size - offset,
			                                     &instruction,
			                                     operands);
			if (!ZYAN_SUCCESS(status))
				return std::nullopt;

			bool terminator = false;
			switch (instruction.meta.category)
			{
				case ZYDIS_CATEGORY_COND_BR:
				case ZYDIS_CATEGORY_UNCOND_BR:
				{
					terminator = true;
					const auto& operand = operands[0];
					if (operand.type != ZYDIS_OPERAND_TYPE_IMMEDIATE)
					{
						// Indirect jump: a switch jump table or similar.
						// Its targets are unknown statically, so no run
						// boundary in this function can be trusted.
						return std::nullopt;
					}

					ZyanU64 target = 0;
					if (!ZYAN_SUCCESS(ZydisCalcAbsoluteAddress(
					        &instruction,
					        &operand,
					        startVirtualAddress + offset,
					        &target)))
					{
						return std::nullopt;
					}
					if (target >= startVirtualAddress &&
					    target < startVirtualAddress + size)
					{
						targets.insert(target);
					}
					break;
				}
				case ZYDIS_CATEGORY_CALL:
					// A callee can throw and never return, so nothing after
					// a call may be assumed executed. The target is another
					// function, never a run boundary here.
				case ZYDIS_CATEGORY_RET:
				case ZYDIS_CATEGORY_INTERRUPT:
					terminator = true;
					break;
				default:
					if (instruction.mnemonic == ZYDIS_MNEMONIC_UD0 ||
					    instruction.mnemonic == ZYDIS_MNEMONIC_UD1 ||
					    instruction.mnemonic == ZYDIS_MNEMONIC_UD2)
					{
						terminator = true;
					}
					break;
			}

			instructionOffsets.push_back(offset);
			endsRun.push_back(terminator);
			offset += instruction.length;
		}

		// A branch target inside another instruction would mean overlapping
		// instruction streams: reject.
		std::unordered_set<DWORD64> instructionStarts;
		instructionStarts.reserve(instructionOffsets.size());
		for (auto instructionOffset : instructionOffsets)
			instructionStarts.insert(startVirtualAddress + instructionOffset);
		for (auto target : targets)
		{
			if (instructionStarts.find(target) == instructionStarts.end())
				return std::nullopt;
		}

		// Pass 2: cut the instruction stream into runs, in address order.
		std::vector<std::pair<DWORD64, DWORD64>> runRanges; // [start, end)
		DWORD64 runStart = 0;
		bool inRun = false;
		for (size_t i = 0; i < instructionOffsets.size(); ++i)
		{
			auto instructionStart =
			    startVirtualAddress + instructionOffsets[i];
			auto instructionEnd =
			    (i + 1 < instructionOffsets.size())
			        ? startVirtualAddress + instructionOffsets[i + 1]
			        : startVirtualAddress + size;

			if (inRun && targets.count(instructionStart))
			{
				runRanges.emplace_back(runStart, instructionStart);
				inRun = false;
			}
			if (!inRun)
			{
				runStart = instructionStart;
				inRun = true;
			}
			if (endsRun[i])
			{
				runRanges.emplace_back(runStart, instructionEnd);
				inRun = false;
			}
		}
		if (inRun)
			runRanges.emplace_back(runStart, startVirtualAddress + size);

		// Pass 3: attach every line to each run its code range intersects.
		// A line spanning several runs is registered under each of their
		// leaders: it is executed when any of them is entered.
		std::vector<LineRun> runs;
		runs.reserve(runRanges.size());
		for (const auto& range : runRanges)
			runs.push_back(LineRun{range.first, {}});

		size_t firstCandidateRun = 0;
		for (size_t lineIndex = 0; lineIndex < lines.size(); ++lineIndex)
		{
			auto lineStart = lines[lineIndex].virtualAddress_;
			auto lineEnd =
			    lineStart +
			    std::max<std::uint32_t>(1, lines[lineIndex].length_);

			while (firstCandidateRun < runRanges.size() &&
			       runRanges[firstCandidateRun].second <= lineStart)
			{
				++firstCandidateRun;
			}
			for (auto run = firstCandidateRun;
			     run < runRanges.size() && runRanges[run].first < lineEnd;
			     ++run)
			{
				runs[run].lineIndices_.push_back(lineIndex);
			}
		}

		runs.erase(std::remove_if(runs.begin(),
		                          runs.end(),
		                          [](const LineRun& run) {
			                          return run.lineIndices_.empty();
		                          }),
		           runs.end());
		return runs;
	}
}
