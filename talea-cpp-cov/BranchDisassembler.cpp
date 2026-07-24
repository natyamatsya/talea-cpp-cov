// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 natyamatsya and talea-cpp-cov contributors

#include "stdafx.h"
#include <cstdint>
#include "BranchDisassembler.hpp"

#include <optional>

#ifndef ZYDIS_STATIC_BUILD
#define ZYDIS_STATIC_BUILD
#endif
#include <zydis/Zydis.h>

#include "tools/Log.hpp"

namespace TaleaCppCov
{
	namespace
	{
		//---------------------------------------------------------------------
		std::optional<BranchCondition>
		GetBranchCondition(ZydisMnemonic mnemonic)
		{
			switch (mnemonic)
			{
				case ZYDIS_MNEMONIC_JO: return BranchCondition::Overflow;
				case ZYDIS_MNEMONIC_JNO: return BranchCondition::NoOverflow;
				case ZYDIS_MNEMONIC_JB: return BranchCondition::Below;
				case ZYDIS_MNEMONIC_JNB: return BranchCondition::AboveOrEqual;
				case ZYDIS_MNEMONIC_JZ: return BranchCondition::Zero;
				case ZYDIS_MNEMONIC_JNZ: return BranchCondition::NotZero;
				case ZYDIS_MNEMONIC_JBE: return BranchCondition::BelowOrEqual;
				case ZYDIS_MNEMONIC_JNBE: return BranchCondition::Above;
				case ZYDIS_MNEMONIC_JS: return BranchCondition::Sign;
				case ZYDIS_MNEMONIC_JNS: return BranchCondition::NoSign;
				case ZYDIS_MNEMONIC_JP: return BranchCondition::Parity;
				case ZYDIS_MNEMONIC_JNP: return BranchCondition::NoParity;
				case ZYDIS_MNEMONIC_JL: return BranchCondition::Less;
				case ZYDIS_MNEMONIC_JNL: return BranchCondition::GreaterOrEqual;
				case ZYDIS_MNEMONIC_JLE: return BranchCondition::LessOrEqual;
				case ZYDIS_MNEMONIC_JNLE: return BranchCondition::Greater;
				case ZYDIS_MNEMONIC_JCXZ: return BranchCondition::CxZero;
				case ZYDIS_MNEMONIC_JECXZ: return BranchCondition::EcxZero;
				case ZYDIS_MNEMONIC_JRCXZ: return BranchCondition::RcxZero;
				default:
					// LOOP/LOOPE/LOOPNE are also conditional branches but
					// involve a pre-decrement of RCX. They are never emitted
					// by MSVC and are excluded on purpose.
					return std::nullopt;
			}
		}
	}

	//-------------------------------------------------------------------------
	class BranchDisassembler::Impl
	{
	  public:
		ZydisDecoder decoder_;
		bool valid_ = false;
	};

	//-------------------------------------------------------------------------
	BranchDisassembler::BranchDisassembler(bool is64BitCode)
	    : impl_{std::make_unique<Impl>()}
	{
		// Full decode (not ZYDIS_DECODER_MODE_MINIMAL): rejecting inline data
		// needs the instruction category and the branch operand type, which
		// the minimal mode does not populate.
		auto status =
		    is64BitCode
		        ? ZydisDecoderInit(&impl_->decoder_,
		                           ZYDIS_MACHINE_MODE_LONG_64,
		                           ZYDIS_STACK_WIDTH_64)
		        : ZydisDecoderInit(&impl_->decoder_,
		                           ZYDIS_MACHINE_MODE_LEGACY_32,
		                           ZYDIS_STACK_WIDTH_32);
		impl_->valid_ = ZYAN_SUCCESS(status);

		if (!impl_->valid_)
			LOG_ERROR("Cannot initialize Zydis decoder.");
	}

	//-------------------------------------------------------------------------
	BranchDisassembler::~BranchDisassembler() = default;

	//-------------------------------------------------------------------------
	std::optional<std::vector<BranchInstruction>>
	BranchDisassembler::FindConditionalBranches(
	    const std::uint8_t* buffer,
	    size_t size,
	    DWORD64 startVirtualAddress) const
	{
		std::vector<BranchInstruction> branches;
		if (!impl_->valid_)
			return std::nullopt;
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
			{
				// Not decodable as code: the range contains data (e.g. an
				// inline switch jump table). Nothing found in it can be
				// trusted as a patch target.
				return std::nullopt;
			}

			if ((instruction.meta.category == ZYDIS_CATEGORY_COND_BR ||
			     instruction.meta.category == ZYDIS_CATEGORY_UNCOND_BR) &&
			    operands[0].type != ZYDIS_OPERAND_TYPE_IMMEDIATE)
			{
				// Indirect jump: a switch jump table or similar, whose table
				// bytes live inline in the function's range (see the header).
				return std::nullopt;
			}

			// Classify by mnemonic: GetBranchCondition returns a value only
			// for the conditional-jump mnemonics and std::nullopt for
			// everything else. LOOP* are excluded there.
			if (auto condition = GetBranchCondition(instruction.mnemonic))
			{
				branches.emplace_back(
				    startVirtualAddress + offset, instruction.length, *condition);
			}
			offset += instruction.length;
		}

		return branches;
	}
}
