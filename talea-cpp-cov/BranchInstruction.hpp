// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 natyamatsya and talea-cpp-cov contributors

#pragma once
#include <cstdint>

#include <Windows.h>

namespace TaleaCppCov
{
	// Condition of a conditional jump instruction (Jcc).
	enum class BranchCondition
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

	// A conditional branch instruction found by disassembling
	// the code range of a source line.
	struct BranchInstruction
	{
		BranchInstruction(DWORD64 virtualAddress,
		                  std::uint8_t length,
		                  BranchCondition condition)
		    : virtualAddress_{virtualAddress},
		      length_{length},
		      condition_{condition}
		{
		}

		DWORD64 virtualAddress_;
		std::uint8_t length_;
		BranchCondition condition_;
	};
}
