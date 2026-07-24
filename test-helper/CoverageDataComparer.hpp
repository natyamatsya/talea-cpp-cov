// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2014 OpenCppCoverage

#pragma once

#include "TestHelperExport.hpp"
#include <memory>

namespace Coverage
{
	class CoverageData;
	class ModuleCoverage;
}

namespace TestHelper
{
	class TEST_HELPER_DLL CoverageDataComparer
	{
	public:
		CoverageDataComparer() = default;

		// compareBranches: when false, only line coverage is compared and the
		// per-line branch (condition) coverage is ignored. Fast-mode import
		// derives branch coverage from block counters, which is approximate at
		// control-flow merges by design, so it cannot be compared for exact
		// equality against the debugger/VEH oracle - only its line coverage can.
		void AssertEquals(const Coverage::CoverageData&,
		                  const Coverage::CoverageData&,
		                  bool compareBranches = true) const;
		void AssertEquals(const Coverage::ModuleCoverage*, const Coverage::ModuleCoverage*) const;

		using ModuleCoveragePtr = std::unique_ptr<Coverage::ModuleCoverage>;
		using ModuleCoverageCollection = std::vector<ModuleCoveragePtr>;

		bool IsFirstModuleContainsSecond(
			const ModuleCoveragePtr& module1,
			const ModuleCoveragePtr& module2) const;

		bool IsFirstCollectionContainsSecond(
			const ModuleCoverageCollection& container1,
			const ModuleCoverageCollection& container2) const;
	private:
		CoverageDataComparer(const CoverageDataComparer&) = delete;
		CoverageDataComparer& operator=(const CoverageDataComparer&) = delete;
	};
}