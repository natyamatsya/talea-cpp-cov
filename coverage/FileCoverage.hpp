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

#pragma once
#include <cstdint>

#include <filesystem>
#include <flat_map>
#include <vector>

#include "LineCoverage.hpp"
#include "CoverageExport.hpp"

namespace Coverage
{
	class COVERAGE_DLL FileCoverage
	{
	public:
		explicit FileCoverage(const std::filesystem::path& path);

		void AddLine(std::uint32_t lineNumber, bool hasBeenExecuted);
		void UpdateLine(std::uint32_t lineNumber, bool hasBeenExecuted);
		void AddBranch(std::uint32_t lineNumber, const BranchCoverage&);
		void MergeBranch(std::uint32_t lineNumber, const BranchCoverage&);

		const std::filesystem::path& GetPath() const;
		const LineCoverage* operator[](std::uint32_t line) const;
		std::vector<LineCoverage> GetLines() const;

		// Zero-copy access to the lines (ordered by line number). Prefer this
		// over GetLines() on hot paths; GetLines() copies every line.
		const std::flat_map<std::uint32_t, LineCoverage>& GetLineCollection() const;

		FileCoverage& operator=(const FileCoverage&) = default;

	private:
		FileCoverage(const FileCoverage&) = delete;
			
	private:
		std::filesystem::path path_;
		std::flat_map<std::uint32_t, LineCoverage> lines_;
	};
}


