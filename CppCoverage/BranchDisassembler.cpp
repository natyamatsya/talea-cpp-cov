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
#include "BranchDisassembler.hpp"

#include <optional>

#define ZYDIS_STATIC_BUILD
#include <Zydis/Zydis.h>

#include "tools/Log.hpp"

namespace CppCoverage
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
			LOG_ERROR_STREAM << "Cannot initialize Zydis decoder.";
	}

	//-------------------------------------------------------------------------
	BranchDisassembler::~BranchDisassembler() = default;

	//-------------------------------------------------------------------------
	std::vector<BranchInstruction> BranchDisassembler::FindConditionalBranches(
	    const unsigned char* buffer,
	    size_t size,
	    DWORD64 startVirtualAddress) const
	{
		std::vector<BranchInstruction> branches;
		if (!impl_->valid_)
			return branches;
		size_t offset = 0;

		while (offset < size)
		{
			ZydisDecodedInstruction instruction;
			auto status = ZydisDecoderDecodeInstruction(&impl_->decoder_,
			                                            nullptr,
			                                            buffer + offset,
			                                            size - offset,
			                                            &instruction);
			if (!ZYAN_SUCCESS(status))
			{
				// Line ranges from the PDB are instruction aligned, so this
				// is a defensive path only: skip one byte and resync.
				++offset;
				continue;
			}

			if (instruction.meta.category == ZYDIS_CATEGORY_COND_BR)
			{
				auto condition = GetBranchCondition(instruction.mnemonic);
				if (condition)
				{
					branches.emplace_back(startVirtualAddress + offset,
					                      instruction.length,
					                      *condition);
				}
			}
			offset += instruction.length;
		}

		return branches;
	}
}
