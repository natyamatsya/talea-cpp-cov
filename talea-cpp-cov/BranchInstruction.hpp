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
