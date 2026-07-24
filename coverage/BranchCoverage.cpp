// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2014 OpenCppCoverage
// SPDX-FileCopyrightText: 2026 natyamatsya and talea-cpp-cov contributors

#include "stdafx.h"
#include <cstdint>
#include "BranchCoverage.hpp"

namespace Coverage
{
	//-------------------------------------------------------------------------
	BranchCoverage::BranchCoverage(std::uint32_t branchIndex,
	                               bool hasBeenTaken,
	                               bool hasBeenFallenThrough)
		: branchIndex_(branchIndex)
		, hasBeenTaken_(hasBeenTaken)
		, hasBeenFallenThrough_(hasBeenFallenThrough)
	{
	}

	//-------------------------------------------------------------------------
	std::uint32_t BranchCoverage::GetBranchIndex() const
	{
		return branchIndex_;
	}

	//-------------------------------------------------------------------------
	bool BranchCoverage::HasBeenTaken() const
	{
		return hasBeenTaken_;
	}

	//-------------------------------------------------------------------------
	bool BranchCoverage::HasBeenFallenThrough() const
	{
		return hasBeenFallenThrough_;
	}
}
