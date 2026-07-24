// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2014 OpenCppCoverage

#include "stdafx.h"
#include "ModuleCoverage.hpp"

#include "FileCoverage.hpp"

namespace Coverage
{
	//-------------------------------------------------------------------------
	ModuleCoverage::ModuleCoverage(const std::filesystem::path& path)
		: path_(path)
	{
	}

	//-------------------------------------------------------------------------
	ModuleCoverage::~ModuleCoverage()
	{
	}

	//-------------------------------------------------------------------------
	FileCoverage& ModuleCoverage::AddFile(const std::filesystem::path& filePath)
	{
		files_.push_back(std::unique_ptr<FileCoverage>(new FileCoverage(filePath)));

		return *files_.back();
	}

	//-------------------------------------------------------------------------
	const std::filesystem::path& ModuleCoverage::GetPath() const
	{
		return path_;
	}

	//-------------------------------------------------------------------------
	const ModuleCoverage::T_FileCoverageCollection& ModuleCoverage::GetFiles() const
	{
		return files_;
	}
}
