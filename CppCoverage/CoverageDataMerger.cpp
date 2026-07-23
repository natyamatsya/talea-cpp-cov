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

#include "Plugin/Exporter/CoverageData.hpp"
#include "Plugin/Exporter/ModuleCoverage.hpp"
#include "Plugin/Exporter/FileCoverage.hpp"
#include "Plugin/Exporter/LineCoverage.hpp"

namespace fs = std::filesystem;

namespace CppCoverage
{
	namespace
	{
		//---------------------------------------------------------------------
		Plugin::CoverageData CreateCoverageData(const std::vector<Plugin::CoverageData>& coverageDataCollection)
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

			return Plugin::CoverageData{ name, lastNotZeroExitCode };
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
			const Plugin::FileCoverage* sourceFile,
			Plugin::FileCoverage* destinationFile)
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
			Plugin::FileCoverage& file,
			const std::vector<Plugin::FileCoverage*>& files)
		{
			for (const auto& f : files)
				AddFileCoverageTo(f, &file);
		}

		//---------------------------------------------------------------------
		void FillModule(
			Plugin::ModuleCoverage& module,
			const std::vector<Plugin::ModuleCoverage*>& modules)
		{
			std::map<fs::path, std::vector<Plugin::FileCoverage*>> filesByPath =
				GroupChildrenByKey<Plugin::ModuleCoverage*, fs::path, Plugin::FileCoverage>(
				modules,
				[](const Plugin::ModuleCoverage* m) -> const Plugin::ModuleCoverage::T_FileCoverageCollection&{ return m->GetFiles(); },
				[](const Plugin::FileCoverage& file) -> const fs::path&{ return file.GetPath(); });

			for (const auto& pair : filesByPath)
			{
				auto& file = module.AddFile(pair.first);
				FillFiles(file, pair.second);
			}
		}

	}

	//-------------------------------------------------------------------------
	Plugin::CoverageData CoverageDataMerger::Merge(
		const std::vector<Plugin::CoverageData>& coverageDataCollection) const
	{
		auto coverageData = CreateCoverageData(coverageDataCollection);

		std::map<fs::path, std::vector<Plugin::ModuleCoverage*>> modulesByPath =
			GroupChildrenByKey<Plugin::CoverageData, fs::path, Plugin::ModuleCoverage>(
				coverageDataCollection,
				[](const Plugin::CoverageData& data) -> const Plugin::CoverageData::T_ModuleCoverageCollection& { return data.GetModules(); },
				[](const Plugin::ModuleCoverage& module) -> const fs::path& { return module.GetPath(); });
		
		for (const auto& pair : modulesByPath)
		{
			auto& module = coverageData.AddModule(pair.first);
			FillModule(module, pair.second);
		}
		
		return coverageData;
	}

	//-------------------------------------------------------------------------
	Plugin::CoverageData CoverageDataMerger::AggregateByFile(
		const Plugin::CoverageData& source) const
	{
		Plugin::CoverageData result{source.GetName(), source.GetExitCode()};

		std::vector<Plugin::ModuleCoverage*> modules;
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
