// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2014 OpenCppCoverage

#pragma once
#include <cstdint>

#include <string>
#include <vector>
#include <memory>
#include <filesystem>

#include "CoverageExport.hpp"

namespace Coverage
{
	class ModuleCoverage;

	class COVERAGE_DLL CoverageData
	{
	public:
		typedef std::vector<std::unique_ptr<ModuleCoverage>> T_ModuleCoverageCollection;

	public:
		explicit CoverageData(const std::wstring& name, std::int32_t exitCode);
		~CoverageData();

		CoverageData(CoverageData&&);			
		CoverageData& operator=(CoverageData&&);
		ModuleCoverage& AddModule(const std::filesystem::path& name);
		
		void SetName(const std::wstring&);
		void SetExitCode(std::int32_t);

		const T_ModuleCoverageCollection& GetModules() const;
		const std::wstring& GetName() const;
		std::int32_t GetExitCode() const;

	private:
		CoverageData(const CoverageData&) = delete;
		CoverageData& operator=(const CoverageData&) = delete;

	private:
		T_ModuleCoverageCollection modules_;
		std::wstring name_;
		std::int32_t exitCode_;
	};
}


