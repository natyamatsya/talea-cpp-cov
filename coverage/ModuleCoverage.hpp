// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2014 OpenCppCoverage

#pragma once

#include <vector>
#include <memory>

#include <filesystem>

#include "CoverageExport.hpp"

namespace Coverage
{
	class FileCoverage;

	class COVERAGE_DLL ModuleCoverage
	{
	public:
		typedef std::vector<std::unique_ptr<FileCoverage>> T_FileCoverageCollection;

	public:
		explicit ModuleCoverage(const std::filesystem::path& path);
		~ModuleCoverage();

		FileCoverage& AddFile(const std::filesystem::path& filename);
		
		const std::filesystem::path& GetPath() const;
		const T_FileCoverageCollection& GetFiles() const;

	private:
		ModuleCoverage(const ModuleCoverage&) = delete;
		ModuleCoverage& operator=(const ModuleCoverage&) = delete;
		
	private:
		T_FileCoverageCollection files_;
		std::filesystem::path path_;		
	};
}


