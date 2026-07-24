// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2014 OpenCppCoverage

#include "stdafx.h"
#include <cstdint>
#include <string>
#include "FileCoverage.hpp"

namespace Coverage
{
	//-------------------------------------------------------------------------
	FileCoverage::FileCoverage(const std::filesystem::path& path)
		: path_(path)
	{
	}

	//-------------------------------------------------------------------------
	void FileCoverage::AddLine(std::uint32_t lineNumber, bool hasBeenExecuted)
	{
		LineCoverage line{ lineNumber, hasBeenExecuted };

		if (!lines_.emplace(lineNumber, line).second)
		{
			throw std::runtime_error("Line " + std::to_string(lineNumber) +
				" already exists for " + path_.string());
		}
	}

	//-------------------------------------------------------------------------
	void FileCoverage::UpdateLine(std::uint32_t lineNumber, bool hasBeenExecuted)
	{
		auto it = lines_.find(lineNumber);

		if (it == lines_.end())
		{
			throw std::runtime_error(
			    "Line " + std::to_string(lineNumber) +
			    " does not exists and cannot be updated for " + path_.string());
		}

		it->second.SetHasBeenExecuted(hasBeenExecuted);
	}

	//-------------------------------------------------------------------------
	void FileCoverage::AddBranch(std::uint32_t lineNumber,
	                             const BranchCoverage& branch)
	{
		auto it = lines_.find(lineNumber);

		if (it == lines_.end())
		{
			throw std::runtime_error(
			    "Line " + std::to_string(lineNumber) +
			    " does not exist and cannot get a branch for " + path_.string());
		}

		it->second.AddBranch(branch);
	}

	//-------------------------------------------------------------------------
	void FileCoverage::MergeBranch(std::uint32_t lineNumber,
	                               const BranchCoverage& branch)
	{
		auto it = lines_.find(lineNumber);

		if (it == lines_.end())
		{
			throw std::runtime_error(
			    "Line " + std::to_string(lineNumber) +
			    " does not exist and cannot merge a branch for " +
			    path_.string());
		}

		it->second.MergeBranch(branch);
	}

	//-------------------------------------------------------------------------
	const std::filesystem::path& FileCoverage::GetPath() const
	{
		return path_;
	}

	//-------------------------------------------------------------------------
	const LineCoverage* FileCoverage::operator[](std::uint32_t line) const
	{
		auto it = lines_.find(line);

		if (it == lines_.end())
			return 0;

		return &it->second;
	}
		
	//-------------------------------------------------------------------------
	const std::flat_map<std::uint32_t, LineCoverage>&
	FileCoverage::GetLineCollection() const
	{
		return lines_;
	}

	//-------------------------------------------------------------------------
	std::vector<LineCoverage> FileCoverage::GetLines() const
	{
		std::vector<LineCoverage> lines;
		
		for (const auto& pair : lines_)
			lines.push_back(pair.second);
		
		return lines;
	}	
}
