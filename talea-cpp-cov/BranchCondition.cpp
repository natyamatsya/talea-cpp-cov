// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 natyamatsya and talea-cpp-cov contributors

#include "stdafx.h"
#include "BranchCondition.hpp"

#include <utility>

namespace TaleaCppCov
{
	namespace
	{
		const DWORD CarryFlag = 0x0001;
		const DWORD ParityFlag = 0x0004;
		const DWORD ZeroFlag = 0x0040;
		const DWORD SignFlag = 0x0080;
		const DWORD OverflowFlag = 0x0800;

		//---------------------------------------------------------------------
		bool IsFlagSet(DWORD eflags, DWORD flag)
		{
			return (eflags & flag) != 0;
		}
	}

	//-------------------------------------------------------------------------
	bool IsBranchTaken(BranchCondition condition, DWORD eflags, DWORD64 rcx)
	{
		const auto carry = IsFlagSet(eflags, CarryFlag);
		const auto parity = IsFlagSet(eflags, ParityFlag);
		const auto zero = IsFlagSet(eflags, ZeroFlag);
		const auto sign = IsFlagSet(eflags, SignFlag);
		const auto overflow = IsFlagSet(eflags, OverflowFlag);

		switch (condition)
		{
			case BranchCondition::Overflow: return overflow;
			case BranchCondition::NoOverflow: return !overflow;
			case BranchCondition::Below: return carry;
			case BranchCondition::AboveOrEqual: return !carry;
			case BranchCondition::Zero: return zero;
			case BranchCondition::NotZero: return !zero;
			case BranchCondition::BelowOrEqual: return carry || zero;
			case BranchCondition::Above: return !carry && !zero;
			case BranchCondition::Sign: return sign;
			case BranchCondition::NoSign: return !sign;
			case BranchCondition::Parity: return parity;
			case BranchCondition::NoParity: return !parity;
			case BranchCondition::Less: return sign != overflow;
			case BranchCondition::GreaterOrEqual: return sign == overflow;
			case BranchCondition::LessOrEqual: return zero || sign != overflow;
			case BranchCondition::Greater: return !zero && sign == overflow;
			case BranchCondition::CxZero: return (rcx & 0xFFFF) == 0;
			case BranchCondition::EcxZero: return (rcx & 0xFFFFFFFF) == 0;
			case BranchCondition::RcxZero: return rcx == 0;
		}

		std::unreachable();
	}
}
