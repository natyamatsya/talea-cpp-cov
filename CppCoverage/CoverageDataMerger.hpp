// OpenCppCoverage is an open source code coverage for C++.
// Copyright (C) 2014 OpenCppCoverage

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

#pragma once

#include "CppCoverageExport.hpp"

namespace Coverage
{
	class CoverageData;
}

namespace CppCoverage
{
	class CPPCOVERAGE_DLL CoverageDataMerger
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