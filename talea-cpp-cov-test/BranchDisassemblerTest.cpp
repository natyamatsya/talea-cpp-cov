// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 natyamatsya and talea-cpp-cov contributors

#include "stdafx.h"
#include <cstdint>

#include "talea-cpp-cov/BranchDisassembler.hpp"

namespace cov = TaleaCppCov;

namespace TaleaCppCovTest
{
	namespace
	{
		const DWORD64 StartAddress = 0x1000;
	}

	//-------------------------------------------------------------------------
	TEST(BranchDisassemblerTest, ShortConditionalJump)
	{
		// cmp eax, ebx; je +0x10; mov eax, 1
		const std::uint8_t buffer[] = {
		    0x3B, 0xC3,             // cmp eax, ebx
		    0x74, 0x10,             // je +0x10
		    0xB8, 0x01, 0x00, 0x00, 0x00 // mov eax, 1
		};

		cov::BranchDisassembler disassembler{true};
		auto branches = disassembler.FindConditionalBranches(
		    buffer, sizeof(buffer), StartAddress);

		ASSERT_TRUE(branches.has_value());
		ASSERT_EQ(1u, branches->size());
		ASSERT_EQ(StartAddress + 2, (*branches)[0].virtualAddress_);
		ASSERT_EQ(2, (*branches)[0].length_);
		ASSERT_EQ(cov::BranchCondition::Zero, (*branches)[0].condition_);
	}

	//-------------------------------------------------------------------------
	TEST(BranchDisassemblerTest, LongConditionalJump)
	{
		// jne near +0x1000
		const std::uint8_t buffer[] = {
		    0x0F, 0x85, 0x00, 0x10, 0x00, 0x00 // jne near
		};

		cov::BranchDisassembler disassembler{true};
		auto branches = disassembler.FindConditionalBranches(
		    buffer, sizeof(buffer), StartAddress);

		ASSERT_TRUE(branches.has_value());
		ASSERT_EQ(1u, branches->size());
		ASSERT_EQ(StartAddress, (*branches)[0].virtualAddress_);
		ASSERT_EQ(6, (*branches)[0].length_);
		ASSERT_EQ(cov::BranchCondition::NotZero, (*branches)[0].condition_);
	}

	//-------------------------------------------------------------------------
	TEST(BranchDisassemblerTest, UnconditionalNotDetected)
	{
		const std::uint8_t buffer[] = {
		    0xEB, 0x10,                   // jmp short
		    0xE9, 0x00, 0x10, 0x00, 0x00, // jmp near
		    0xE8, 0x00, 0x10, 0x00, 0x00, // call
		    0xC3                          // ret
		};

		cov::BranchDisassembler disassembler{true};
		auto branches = disassembler.FindConditionalBranches(
		    buffer, sizeof(buffer), StartAddress);

		ASSERT_TRUE(branches.has_value());
		ASSERT_TRUE(branches->empty());
	}

	//-------------------------------------------------------------------------
	TEST(BranchDisassemblerTest, ConditionalMoveNotDetected)
	{
		const std::uint8_t buffer[] = {
		    0x0F, 0x44, 0xC1, // cmove eax, ecx
		    0x0F, 0x94, 0xC0  // sete al
		};

		cov::BranchDisassembler disassembler{true};
		auto branches = disassembler.FindConditionalBranches(
		    buffer, sizeof(buffer), StartAddress);

		ASSERT_TRUE(branches.has_value());
		ASSERT_TRUE(branches->empty());
	}

	//-------------------------------------------------------------------------
	TEST(BranchDisassemblerTest, SeveralConditions)
	{
		const std::uint8_t buffer[] = {
		    0x72, 0x02, // jb +2
		    0x77, 0x00, // ja +0
		    0x7C, 0x00, // jl +0
		    0x7F, 0x00  // jg +0
		};

		cov::BranchDisassembler disassembler{true};
		auto branches = disassembler.FindConditionalBranches(
		    buffer, sizeof(buffer), StartAddress);

		ASSERT_TRUE(branches.has_value());
		ASSERT_EQ(4u, branches->size());
		ASSERT_EQ(cov::BranchCondition::Below, (*branches)[0].condition_);
		ASSERT_EQ(cov::BranchCondition::Above, (*branches)[1].condition_);
		ASSERT_EQ(cov::BranchCondition::Less, (*branches)[2].condition_);
		ASSERT_EQ(cov::BranchCondition::Greater, (*branches)[3].condition_);
	}

	//-------------------------------------------------------------------------
	TEST(BranchDisassemblerTest, X86Mode)
	{
		// dec eax (x86 one byte encoding); jnz -3
		const std::uint8_t buffer[] = {
		    0x48,       // dec eax in 32 bits mode (REX prefix in 64 bits mode)
		    0x75, 0xFD  // jnz -3
		};

		cov::BranchDisassembler disassembler{false};
		auto branches = disassembler.FindConditionalBranches(
		    buffer, sizeof(buffer), StartAddress);

		ASSERT_TRUE(branches.has_value());
		ASSERT_EQ(1u, branches->size());
		ASSERT_EQ(StartAddress + 1, (*branches)[0].virtualAddress_);
		ASSERT_EQ(cov::BranchCondition::NotZero, (*branches)[0].condition_);
	}

	//-------------------------------------------------------------------------
	TEST(BranchDisassemblerTest, IndirectJumpRejectsFunction)
	{
		// The dispatch shape MSVC emits for a switch jump table: a range
		// guard, an indirect jmp through the table, and the table DATA inline
		// after the code. Everything must be rejected - a breakpoint patched
		// into the table bytes corrupts the dispatch.
		const std::uint8_t buffer[] = {
		    0x83, 0xF9, 0x12,             // cmp ecx, 18
		    0x77, 0x02,                   // ja +2
		    0xFF, 0xE0,                   // jmp rax   (indirect)
		    0xC3,                         // ret
		    0xA5, 0x62, 0x00, 0x00,       // inline jump-table data
		    0xA8, 0x62, 0x00, 0x00
		};

		cov::BranchDisassembler disassembler{true};
		auto branches = disassembler.FindConditionalBranches(
		    buffer, sizeof(buffer), StartAddress);

		ASSERT_FALSE(branches.has_value());
	}

	//-------------------------------------------------------------------------
	TEST(BranchDisassemblerTest, UndecodableBytesRejectFunction)
	{
		// A conditional jump followed by bytes that cannot decode as x64 code
		// (inline data): the whole range is rejected, including the real Jcc
		// before the data.
		const std::uint8_t buffer[] = {
		    0x74, 0x02,      // je +2
		    0x62, 0x00, 0x00 // invalid EVEX encoding in 64-bit mode
		};

		cov::BranchDisassembler disassembler{true};
		auto branches = disassembler.FindConditionalBranches(
		    buffer, sizeof(buffer), StartAddress);

		ASSERT_FALSE(branches.has_value());
	}
}
