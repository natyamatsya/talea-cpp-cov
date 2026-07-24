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
#include "CoverageDataMerger.hpp"

#include <functional>
#include <map>

#include "Coverage/CoverageData.hpp"
#include "Coverage/ModuleCoverage.hpp"
#include "Coverage/FileCoverage.hpp"
#include "Coverage/LineCoverage.hpp"

namespace fs = std::filesystem;

namespace TaleaCppCov
{
	namespace
	{
		//---------------------------------------------------------------------
		Coverage::CoverageData CreateCoverageData(const std::vector<Coverage::CoverageData>& coverageDataCollection)
		{
			std::wstring name;
			std::int32_t lastNotZeroExitCode = 0;

			for (const auto& coverageData : coverageDataCollection)
			{
				name = coverageData.GetName();
				auto exitCode = coverageData.GetExitCode();
				if (exitCode)
					lastNotZeroExitCode = exitCode;
			}

			return Coverage::CoverageData{ name, lastNotZeroExitCode };
		}
		
		//---------------------------------------------------------------------
		template <typename Object, typename Key, typename Child>
		std::map<Key, std::vector<Child*>> GroupChildrenByKey(
			const std::vector<Object>& collection,
			const std::function<const std::vector<std::unique_ptr<Child>>& (const Object&)>& getChildren,
			const std::function<const Key& (const Child&)>& getKey)
		{			
			std::map<Key, std::vector<Child*>> childrenByKey;

			for (const auto& object : collection)
			{					
				for (const auto& child : getChildren(object))
				{
					const auto& key = getKey(*child);

					childrenByKey[key].push_back(child.get());
				}
			}

			return childrenByKey;
		}
		
		//---------------------------------------------------------------------
		void AddFileCoverageTo(
			const Coverage::FileCoverage* sourceFile,
			Coverage::FileCoverage* destinationFile)
		{
			if (sourceFile && destinationFile)
			{
				for (const auto& [lineNumber, line] : sourceFile->GetLineCollection())
				{
					auto hasBeenExecuted = line.HasBeenExecuted();

					if (!(*destinationFile)[lineNumber])
						destinationFile->AddLine(lineNumber, hasBeenExecuted);
					else if (hasBeenExecuted)
						destinationFile->UpdateLine(lineNumber, true);

					for (const auto& branch : line.GetBranches())
						destinationFile->MergeBranch(lineNumber, branch);
				}
			}
		}

		//---------------------------------------------------------------------
		void FillFiles(
			Coverage::FileCoverage& file,
			const std::vector<Coverage::FileCoverage*>& files)
		{
			for (const auto& f : files)
				AddFileCoverageTo(f, &file);
		}

		//---------------------------------------------------------------------
		void FillModule(
			Coverage::ModuleCoverage& module,
			const std::vector<Coverage::ModuleCoverage*>& modules)
		{
			std::map<fs::path, std::vector<Coverage::FileCoverage*>> filesByPath =
				GroupChildrenByKey<Coverage::ModuleCoverage*, fs::path, Coverage::FileCoverage>(
				modules,
				[](const Coverage::ModuleCoverage* m) -> const Coverage::ModuleCoverage::T_FileCoverageCollection&{ return m->GetFiles(); },
				[](const Coverage::FileCoverage& file) -> const fs::path&{ return file.GetPath(); });

			for (const auto& pair : filesByPath)
			{
				auto& file = module.AddFile(pair.first);
				FillFiles(file, pair.second);
			}
		}

	}

	//-------------------------------------------------------------------------
	Coverage::CoverageData CoverageDataMerger::Merge(
		const std::vector<Coverage::CoverageData>& coverageDataCollection) const
	{
		auto coverageData = CreateCoverageData(coverageDataCollection);

		std::map<fs::path, std::vector<Coverage::ModuleCoverage*>> modulesByPath =
			GroupChildrenByKey<Coverage::CoverageData, fs::path, Coverage::ModuleCoverage>(
				coverageDataCollection,
				[](const Coverage::CoverageData& data) -> const Coverage::CoverageData::T_ModuleCoverageCollection& { return data.GetModules(); },
				[](const Coverage::ModuleCoverage& module) -> const fs::path& { return module.GetPath(); });
		
		for (const auto& pair : modulesByPath)
		{
			auto& module = coverageData.AddModule(pair.first);
			FillModule(module, pair.second);
		}
		
		return coverageData;
	}

	//-------------------------------------------------------------------------
	Coverage::CoverageData CoverageDataMerger::AggregateByFile(
		const Coverage::CoverageData& source) const
	{
		Coverage::CoverageData result{source.GetName(), source.GetExitCode()};

		std::vector<Coverage::ModuleCoverage*> modules;
		modules.reserve(source.GetModules().size());
		for (const auto& module : source.GetModules())
			modules.push_back(module.get());

		// A single module (empty path: the grouping is by source file now, not
		// by binary) whose files are the per-path union across every input
		// module. FillModule already groups files by path and merges them.
		auto& mergedModule = result.AddModule(std::filesystem::path{});
		FillModule(mergedModule, modules);

		return result;
	}
}
