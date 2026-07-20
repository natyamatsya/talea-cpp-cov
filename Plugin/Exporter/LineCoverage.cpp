// OpenCppCoverage is an open source code coverage for C++.
// Copyright (C) 2014 OpenCppCoverage
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
#include "LineCoverage.hpp"

namespace Plugin
{
	//-------------------------------------------------------------------------
	LineCoverage::LineCoverage(unsigned int lineNumber, bool hasBeenExecuted)
		: lineNumber_(lineNumber)
		, hasBeenExecuted_(hasBeenExecuted)
	{
	}
		
	//-------------------------------------------------------------------------
	unsigned int LineCoverage::GetLineNumber() const
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
