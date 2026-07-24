// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2014 OpenCppCoverage

#pragma once
#include <cstdint>

#include <vector>

#include "CoverageExport.hpp"
#include "BranchCoverage.hpp"

namespace Coverage
{
	class COVERAGE_DLL LineCoverage
	{
	public:
		LineCoverage(std::uint32_t lineNumber, bool hasBeenExecuted);
		LineCoverage(const LineCoverage&) = default;
		LineCoverage& operator=(const LineCoverage&) = default;

		std::uint32_t GetLineNumber() const;
		bool HasBeenExecuted() const;
		void SetHasBeenExecuted(bool);

		void AddBranch(const BranchCoverage&);
		void MergeBranch(const BranchCoverage&);
		const std::vector<BranchCoverage>& GetBranches() const;

	private:
		std::uint32_t lineNumber_;
		bool hasBeenExecuted_;
		std::vector<BranchCoverage> branches_;
	};
}


