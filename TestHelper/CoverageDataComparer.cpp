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

#include "stdafx.h"
#include <cstdint>

#include "CoverageDataComparer.hpp"
#include "Coverage/CoverageData.hpp"
#include "Coverage/ModuleCoverage.hpp"
#include "Coverage/FileCoverage.hpp"
#include "Coverage/LineCoverage.hpp"

#include "Container.hpp"

namespace TestHelper
{
	namespace
	{
		//---------------------------------------------------------------------
		template <typename T>
		void AssertEqual(T&& v1, T&& v2)
		{
			EXPECT_EQ(v1, v2);
			if (v1 != v2)
				throw std::runtime_error("Values are not equals.");
		}

		//---------------------------------------------------------------------
		void AssertBranchesEquals(
			const Coverage::BranchCoverage& branch1,
			const Coverage::BranchCoverage& branch2)
		{
			AssertEqual(branch1.GetBranchIndex(), branch2.GetBranchIndex());
			AssertEqual(branch1.HasBeenTaken(), branch2.HasBeenTaken());
			AssertEqual(branch1.HasBeenFallenThrough(),
			            branch2.HasBeenFallenThrough());
		}

		//---------------------------------------------------------------------
		void AssertLinesEquals(
			const Coverage::LineCoverage& line1,
			const Coverage::LineCoverage& line2)
		{
			AssertEqual(line1.GetLineNumber(), line2.GetLineNumber());
			AssertEqual(line1.HasBeenExecuted(), line2.HasBeenExecuted());
			AssertContainerEqual(
			    line1.GetBranches(), line2.GetBranches(), AssertBranchesEquals);
		}

		//---------------------------------------------------------------------
		void AssertFilesEquals(
			const Coverage::FileCoverage& file1,
			const Coverage::FileCoverage& file2)
		{
			AssertEqual(file1.GetPath(), file2.GetPath());
			AssertContainerEqual(file1.GetLines(), file2.GetLines(), AssertLinesEquals);
		}

		//---------------------------------------------------------------------
		void AssertModulesEquals(
			const Coverage::ModuleCoverage& module1,
			const Coverage::ModuleCoverage& module2)
		{
			AssertEqual(module1.GetPath(), module2.GetPath());
			AssertContainerUniquePtrEqual(module1.GetFiles(), module2.GetFiles(), AssertFilesEquals);
		}
	}

	//-------------------------------------------------------------------------
	void CoverageDataComparer::AssertEquals(
		const Coverage::CoverageData& coverageData,
		const Coverage::CoverageData& coverageDataRestored) const
	{
		AssertEqual(coverageData.GetName(), coverageDataRestored.GetName());
		AssertEqual(coverageData.GetExitCode(), coverageDataRestored.GetExitCode());

		AssertContainerUniquePtrEqual(
			coverageData.GetModules(),
			coverageDataRestored.GetModules(),
			AssertModulesEquals);
	}

	//---------------------------------------------------------------------
	void CoverageDataComparer::AssertEquals(
		const Coverage::ModuleCoverage* module1,
		const Coverage::ModuleCoverage* module2) const
	{
		if (!module1 || !module2)
			throw std::runtime_error("Module is null.");
		AssertModulesEquals(*module1, *module2);
	}

	using FileCoveragePtr = std::unique_ptr<Coverage::FileCoverage>;

	//---------------------------------------------------------------------
	bool CoverageDataComparer::IsFirstModuleContainsSecond(
		const ModuleCoveragePtr& module1,
		const ModuleCoveragePtr& module2) const
	{
		if (module1->GetPath() != module2->GetPath())
			return false;

		return IsFirstContainsSecond<std::filesystem::path>(
			module1->GetFiles(), module2->GetFiles(),
			[](const FileCoveragePtr& file) { return file->GetPath(); },
			[](const FileCoveragePtr& file1, const FileCoveragePtr& file2)
		{
			if (file1->GetPath() != file2->GetPath())
				return false;

			return IsFirstContainsSecond<std::uint32_t>(
				file1->GetLines(), file2->GetLines(),
				[](const Coverage::LineCoverage& line) { return line.GetLineNumber(); },
				[](const Coverage::LineCoverage& line1, const Coverage::LineCoverage& line2)
			{
				return !line2.HasBeenExecuted() || line1.HasBeenExecuted();
			});
		});
	}

	//---------------------------------------------------------------------
	bool CoverageDataComparer::IsFirstCollectionContainsSecond(
		const ModuleCoverageCollection& container1,
		const ModuleCoverageCollection& container2) const
	{
		return IsFirstContainsSecond<std::filesystem::path>(
			container1, container2,
			[](const ModuleCoveragePtr& module) { return module->GetPath(); },
			[this](const ModuleCoveragePtr& module1,
			   const ModuleCoveragePtr& module2)
		{
			return IsFirstModuleContainsSecond(module1, module2);
		});
	}
}
