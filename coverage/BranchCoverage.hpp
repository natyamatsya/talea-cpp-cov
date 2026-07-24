// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2014 OpenCppCoverage
// SPDX-FileCopyrightText: 2026 natyamatsya and talea-cpp-cov contributors

#pragma once
#include <cstdint>

#include "CoverageExport.hpp"

namespace Coverage
{
	// Coverage of a single conditional branch (two outcomes:
	// branch taken and fall through).
	class COVERAGE_DLL BranchCoverage
	{
	public:
		BranchCoverage(std::uint32_t branchIndex,
		               bool hasBeenTaken,
		               bool hasBeenFallenThrough);
		BranchCoverage(const BranchCoverage&) = default;
		BranchCoverage& operator=(const BranchCoverage&) = default;

		std::uint32_t GetBranchIndex() const;
		bool HasBeenTaken() const;
		bool HasBeenFallenThrough() const;

	private:
		std::uint32_t branchIndex_;
		bool hasBeenTaken_;
		bool hasBeenFallenThrough_;
	};
}
