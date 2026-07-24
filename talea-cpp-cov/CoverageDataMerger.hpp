// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2014 OpenCppCoverage

#pragma once

#include "TaleaCppCovExport.hpp"

namespace Coverage
{
	class CoverageData;
}

namespace TaleaCppCov
{
	class TALEACPPCOV_DLL CoverageDataMerger
	{
	public:
		CoverageDataMerger() = default;
		
		Coverage::CoverageData Merge(const std::vector<Coverage::CoverageData>&) const;

		// Collapse per-module coverage into one entry per source file: the
		// result has a SINGLE module whose files are the distinct source paths
		// across all input modules, each carrying the union of the modules'
		// line and branch coverage (a line is covered if ANY module covered
		// it). This is what --aggregate_by_file produces. Statically-linked
		// shared code compiled into many binaries otherwise appears once per
		// binary: on a test-suite build tree (~one exe per test) that inflates
		// the report by the executable count - both the numbers (a source
		// file's lines counted N times) and the size (the same <class> emitted
		// N times). Collapsing gives the true project figures and shrinks the
		// Cobertura by ~N.
		Coverage::CoverageData AggregateByFile(const Coverage::CoverageData&) const;

	private:
		CoverageDataMerger(const CoverageDataMerger&) = delete;
		CoverageDataMerger& operator=(const CoverageDataMerger&) = delete;
	};
}