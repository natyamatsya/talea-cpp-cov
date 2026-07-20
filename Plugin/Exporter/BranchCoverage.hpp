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

#include "../PluginExport.hpp"

namespace Plugin
{
	// Coverage of a single conditional branch (two outcomes:
	// branch taken and fall through).
	class PLUGIN_DLL BranchCoverage
	{
	public:
		BranchCoverage(unsigned int branchIndex,
		               bool hasBeenTaken,
		               bool hasBeenFallenThrough);
		BranchCoverage(const BranchCoverage&) = default;
		BranchCoverage& operator=(const BranchCoverage&) = default;

		unsigned int GetBranchIndex() const;
		bool HasBeenTaken() const;
		bool HasBeenFallenThrough() const;

	private:
		unsigned int branchIndex_;
		bool hasBeenTaken_;
		bool hasBeenFallenThrough_;
	};
}
