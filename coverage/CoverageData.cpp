// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2014 OpenCppCoverage

#include "stdafx.h"
#include <cstdint>
#include "CoverageData.hpp"

#include "ModuleCoverage.hpp"

namespace Coverage
{
	//-------------------------------------------------------------------------
	CoverageData::CoverageData(const std::wstring& name, std::int32_t exitCode)
		: name_(name)
		, exitCode_(exitCode)
	{
	}

	//-------------------------------------------------------------------------
	CoverageData::~CoverageData()
	{
	}

	//-------------------------------------------------------------------------
	CoverageData::CoverageData(CoverageData&& coverageData)
	{
		*this = std::move(coverageData);
	}

	//-------------------------------------------------------------------------
	CoverageData& CoverageData::operator=(CoverageData&& coverageData)
	{
		if (this != &coverageData)
		{
			std::swap(modules_, coverageData.modules_);
			name_ = coverageData.name_;
			exitCode_ = coverageData.exitCode_;
		}
		return *this;
	}

	//-------------------------------------------------------------------------
	ModuleCoverage& CoverageData::AddModule(const std::filesystem::path& path)
	{
		modules_.push_back(std::unique_ptr<ModuleCoverage>(new ModuleCoverage(path)));

		return *modules_.back();
	}

	//-------------------------------------------------------------------------	
	void CoverageData::SetName(const std::wstring& name)
	{
		name_ = name;
	}

	//-------------------------------------------------------------------------
	void CoverageData::SetExitCode(std::int32_t exitCode)
	{
		exitCode_ = exitCode;
	}

	//-------------------------------------------------------------------------
	const CoverageData::T_ModuleCoverageCollection& CoverageData::GetModules() const
	{
		return modules_;
	}
	
	//-------------------------------------------------------------------------	
	const std::wstring& CoverageData::GetName() const
	{
		return name_;
	}
		
	//-------------------------------------------------------------------------	
	std::int32_t CoverageData::GetExitCode() const
	{
		return exitCode_;
	}
}

