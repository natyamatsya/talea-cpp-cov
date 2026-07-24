// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 natyamatsya and talea-cpp-cov contributors

#include "stdafx.h"

#include "talea-cpp-cov/BranchCondition.hpp"

namespace cov = TaleaCppCov;

namespace TaleaCppCovTest
{
	namespace
	{
		const DWORD CarryFlag = 0x0001;
		const DWORD ParityFlag = 0x0004;
		const DWORD ZeroFlag = 0x0040;
		const DWORD SignFlag = 0x0080;
		const DWORD OverflowFlag = 0x0800;
	}

	//-------------------------------------------------------------------------
	TEST(BranchConditionTest, SimpleFlags)
	{
		ASSERT_TRUE(cov::IsBranchTaken(cov::BranchCondition::Overflow, OverflowFlag, 0));
		ASSERT_FALSE(cov::IsBranchTaken(cov::BranchCondition::Overflow, 0, 0));
		ASSERT_TRUE(cov::IsBranchTaken(cov::BranchCondition::NoOverflow, 0, 0));
		ASSERT_FALSE(cov::IsBranchTaken(cov::BranchCondition::NoOverflow, OverflowFlag, 0));

		ASSERT_TRUE(cov::IsBranchTaken(cov::BranchCondition::Below, CarryFlag, 0));
		ASSERT_FALSE(cov::IsBranchTaken(cov::BranchCondition::Below, 0, 0));
		ASSERT_TRUE(cov::IsBranchTaken(cov::BranchCondition::AboveOrEqual, 0, 0));
		ASSERT_FALSE(cov::IsBranchTaken(cov::BranchCondition::AboveOrEqual, CarryFlag, 0));

		ASSERT_TRUE(cov::IsBranchTaken(cov::BranchCondition::Zero, ZeroFlag, 0));
		ASSERT_FALSE(cov::IsBranchTaken(cov::BranchCondition::Zero, 0, 0));
		ASSERT_TRUE(cov::IsBranchTaken(cov::BranchCondition::NotZero, 0, 0));
		ASSERT_FALSE(cov::IsBranchTaken(cov::BranchCondition::NotZero, ZeroFlag, 0));

		ASSERT_TRUE(cov::IsBranchTaken(cov::BranchCondition::Sign, SignFlag, 0));
		ASSERT_FALSE(cov::IsBranchTaken(cov::BranchCondition::Sign, 0, 0));
		ASSERT_TRUE(cov::IsBranchTaken(cov::BranchCondition::NoSign, 0, 0));
		ASSERT_FALSE(cov::IsBranchTaken(cov::BranchCondition::NoSign, SignFlag, 0));

		ASSERT_TRUE(cov::IsBranchTaken(cov::BranchCondition::Parity, ParityFlag, 0));
		ASSERT_FALSE(cov::IsBranchTaken(cov::BranchCondition::Parity, 0, 0));
		ASSERT_TRUE(cov::IsBranchTaken(cov::BranchCondition::NoParity, 0, 0));
		ASSERT_FALSE(cov::IsBranchTaken(cov::BranchCondition::NoParity, ParityFlag, 0));
	}

	//-------------------------------------------------------------------------
	TEST(BranchConditionTest, UnsignedComparisons)
	{
		// JBE: CF or ZF
		ASSERT_TRUE(cov::IsBranchTaken(cov::BranchCondition::BelowOrEqual, CarryFlag, 0));
		ASSERT_TRUE(cov::IsBranchTaken(cov::BranchCondition::BelowOrEqual, ZeroFlag, 0));
		ASSERT_FALSE(cov::IsBranchTaken(cov::BranchCondition::BelowOrEqual, 0, 0));

		// JA: not CF and not ZF
		ASSERT_TRUE(cov::IsBranchTaken(cov::BranchCondition::Above, 0, 0));
		ASSERT_FALSE(cov::IsBranchTaken(cov::BranchCondition::Above, CarryFlag, 0));
		ASSERT_FALSE(cov::IsBranchTaken(cov::BranchCondition::Above, ZeroFlag, 0));
	}

	//-------------------------------------------------------------------------
	TEST(BranchConditionTest, SignedComparisons)
	{
		// JL: SF != OF
		ASSERT_TRUE(cov::IsBranchTaken(cov::BranchCondition::Less, SignFlag, 0));
		ASSERT_TRUE(cov::IsBranchTaken(cov::BranchCondition::Less, OverflowFlag, 0));
		ASSERT_FALSE(cov::IsBranchTaken(cov::BranchCondition::Less, 0, 0));
		ASSERT_FALSE(cov::IsBranchTaken(cov::BranchCondition::Less, SignFlag | OverflowFlag, 0));

		// JGE: SF == OF
		ASSERT_TRUE(cov::IsBranchTaken(cov::BranchCondition::GreaterOrEqual, 0, 0));
		ASSERT_TRUE(cov::IsBranchTaken(cov::BranchCondition::GreaterOrEqual, SignFlag | OverflowFlag, 0));
		ASSERT_FALSE(cov::IsBranchTaken(cov::BranchCondition::GreaterOrEqual, SignFlag, 0));

		// JLE: ZF or SF != OF
		ASSERT_TRUE(cov::IsBranchTaken(cov::BranchCondition::LessOrEqual, ZeroFlag, 0));
		ASSERT_TRUE(cov::IsBranchTaken(cov::BranchCondition::LessOrEqual, SignFlag, 0));
		ASSERT_FALSE(cov::IsBranchTaken(cov::BranchCondition::LessOrEqual, 0, 0));

		// JG: not ZF and SF == OF
		ASSERT_TRUE(cov::IsBranchTaken(cov::BranchCondition::Greater, 0, 0));
		ASSERT_TRUE(cov::IsBranchTaken(cov::BranchCondition::Greater, SignFlag | OverflowFlag, 0));
		ASSERT_FALSE(cov::IsBranchTaken(cov::BranchCondition::Greater, ZeroFlag, 0));
		ASSERT_FALSE(cov::IsBranchTaken(cov::BranchCondition::Greater, SignFlag, 0));
	}

	//-------------------------------------------------------------------------
	TEST(BranchConditionTest, CxZero)
	{
		ASSERT_TRUE(cov::IsBranchTaken(cov::BranchCondition::CxZero, 0, 0));
		ASSERT_TRUE(cov::IsBranchTaken(cov::BranchCondition::CxZero, 0, 0xFFFF0000));
		ASSERT_FALSE(cov::IsBranchTaken(cov::BranchCondition::CxZero, 0, 1));

		ASSERT_TRUE(cov::IsBranchTaken(cov::BranchCondition::EcxZero, 0, 0));
		ASSERT_TRUE(cov::IsBranchTaken(cov::BranchCondition::EcxZero, 0, 0xFFFFFFFF00000000));
		ASSERT_FALSE(cov::IsBranchTaken(cov::BranchCondition::EcxZero, 0, 1));

		ASSERT_TRUE(cov::IsBranchTaken(cov::BranchCondition::RcxZero, 0, 0));
		ASSERT_FALSE(cov::IsBranchTaken(cov::BranchCondition::RcxZero, 0, 0xFFFFFFFF00000000));
	}
}
