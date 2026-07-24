// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 natyamatsya and talea-cpp-cov contributors

#pragma once

#include <cstdint>

// Branch-condition evaluation shared between the TaleaCppCovApp host and the
// injected VEH helper. Header-only and dependency-free (no CRT, no Windows.h)
// so the tiny helper DLL can evaluate a conditional jump's direction in-process
// from the trapped thread's flags, exactly as the debugger engine does out of
// process in talea-cpp-cov/BranchCondition.cpp.
//
// The enumerator order mirrors TaleaCppCov::BranchCondition; the host maps its
// own enum onto this one (with a static_assert guard) before writing it into
// the shared section, so both sides agree on the byte value.
namespace VehCoverage
{
	enum class BranchCondition : std::uint8_t
	{
		Overflow,       // JO
		NoOverflow,     // JNO
		Below,          // JB, JC, JNAE
		AboveOrEqual,   // JAE, JNB, JNC
		Zero,           // JZ, JE
		NotZero,        // JNZ, JNE
		BelowOrEqual,   // JBE, JNA
		Above,          // JA, JNBE
		Sign,           // JS
		NoSign,         // JNS
		Parity,         // JP, JPE
		NoParity,       // JNP, JPO
		Less,           // JL, JNGE
		GreaterOrEqual, // JGE, JNL
		LessOrEqual,    // JLE, JNG
		Greater,        // JG, JNLE
		CxZero,         // JCXZ
		EcxZero,        // JECXZ
		RcxZero         // JRCXZ
	};

	//-------------------------------------------------------------------------
	// True if a Jcc with the given condition would jump, given the trapped
	// thread's EFLAGS (and RCX for the Jcxz family). Same predicates as
	// TaleaCppCov::IsBranchTaken.
	inline bool
	IsBranchTaken(BranchCondition condition, std::uint32_t eflags, std::uint64_t rcx) noexcept
	{
		constexpr std::uint32_t CarryFlag = 0x0001;
		constexpr std::uint32_t ParityFlag = 0x0004;
		constexpr std::uint32_t ZeroFlag = 0x0040;
		constexpr std::uint32_t SignFlag = 0x0080;
		constexpr std::uint32_t OverflowFlag = 0x0800;

		const bool carry = (eflags & CarryFlag) != 0;
		const bool parity = (eflags & ParityFlag) != 0;
		const bool zero = (eflags & ZeroFlag) != 0;
		const bool sign = (eflags & SignFlag) != 0;
		const bool overflow = (eflags & OverflowFlag) != 0;

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
		return false;
	}

	// Outcome bits written by the helper into BranchProbe::outcome_.
	constexpr std::uint8_t BranchTaken = 0x1;
	constexpr std::uint8_t BranchFallThrough = 0x2;
}
