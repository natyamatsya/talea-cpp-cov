// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2014 OpenCppCoverage

#include "stdafx.h"
#include <cstdint>
#include "LineCoverage.hpp"

namespace Coverage
{
	//-------------------------------------------------------------------------
	LineCoverage::LineCoverage(std::uint32_t lineNumber, bool hasBeenExecuted)
		: lineNumber_(lineNumber)
		, hasBeenExecuted_(hasBeenExecuted)
	{
	}
		
	//-------------------------------------------------------------------------
	std::uint32_t LineCoverage::GetLineNumber() const
	{
		return lineNumber_;
	}

	//-------------------------------------------------------------------------
	bool LineCoverage::HasBeenExecuted() const
	{
		return hasBeenExecuted_;
	}

	//-------------------------------------------------------------------------
	void LineCoverage::SetHasBeenExecuted(bool hasBeenExecuted)
	{
		hasBeenExecuted_ = hasBeenExecuted;
	}

	//-------------------------------------------------------------------------
	void LineCoverage::AddBranch(const BranchCoverage& branch)
	{
		branches_.push_back(branch);
	}

	//-------------------------------------------------------------------------
	void LineCoverage::MergeBranch(const BranchCoverage& branch)
	{
		for (auto& existingBranch : branches_)
		{
			if (existingBranch.GetBranchIndex() == branch.GetBranchIndex())
			{
				existingBranch = BranchCoverage{
				    branch.GetBranchIndex(),
				    existingBranch.HasBeenTaken() || branch.HasBeenTaken(),
				    existingBranch.HasBeenFallenThrough() ||
				        branch.HasBeenFallenThrough()};
				return;
			}
		}
		branches_.push_back(branch);
	}

	//-------------------------------------------------------------------------
	const std::vector<BranchCoverage>& LineCoverage::GetBranches() const
	{
		return branches_;
	}
}
