// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2014 OpenCppCoverage

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


