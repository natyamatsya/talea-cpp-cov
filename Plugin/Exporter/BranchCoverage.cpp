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
#include <cstdint>
#include "BranchCoverage.hpp"

namespace Plugin
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
