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
#include "ExecutedAddressManager.hpp"

#include <algorithm>
#include <map>

#include <boost/container/small_vector.hpp>

#include "Tools/Log.hpp"

#include "CppCoverageException.hpp"
#include "Plugin/Exporter/ModuleCoverage.hpp"
#include "Plugin/Exporter/FileCoverage.hpp"
#include "Address.hpp"

namespace CppCoverage
{
	//-------------------------------------------------------------------------
	struct ExecutedAddressManager::AddressData
	{
		AddressData(std::uint8_t instructionToRestore, void* dllBaseOfImage)
			: instructionToRestore_{instructionToRestore}
			, dllBaseOfImage_{dllBaseOfImage}
		{
		}

		std::uint8_t instructionToRestore_;
		void* dllBaseOfImage_;
		// Probe ids reached by this address (indices, stable across growth).
		boost::container::small_vector<std::uint32_t, 1> probes_;
	};

	//-------------------------------------------------------------------------
	std::size_t ExecutedAddressManager::ProbeKeyHasher::operator()(
	    const ProbeKey& key) const noexcept
	{
		std::size_t seed = key.moduleId;
		seed ^= static_cast<std::size_t>(key.fileId) + 0x9E3779B97F4A7C15ULL +
		        (seed << 6) + (seed >> 2);
		seed ^= static_cast<std::size_t>(key.lineNumber) +
		        0x9E3779B97F4A7C15ULL + (seed << 6) + (seed >> 2);
		return seed;
	}

	//-------------------------------------------------------------------------
	ExecutedAddressManager::ExecutedAddressManager() = default;

	//-------------------------------------------------------------------------
	ExecutedAddressManager::~ExecutedAddressManager() = default;

	//-------------------------------------------------------------------------
	void ExecutedAddressManager::AddModule(
		const std::wstring& moduleName,
		void* dllBaseOfImage)
	{
		auto it = moduleIdByName_.find(moduleName);
		if (it == moduleIdByName_.end())
		{
			auto id = static_cast<std::uint32_t>(moduleIdByName_.size());
			it = moduleIdByName_.emplace(moduleName, id).first;
		}

		lastModuleId_ = it->second;
		lastBaseOfImage_ = dllBaseOfImage;
		hasLastModule_ = true;
	}

	//-------------------------------------------------------------------------
	std::uint32_t ExecutedAddressManager::InternFile(
	    const std::wstring& filename)
	{
		// Registration delivers all of a file's addresses together, so the
		// filename usually repeats: skip the hash lookup for a repeat.
		if (hasLastFile_ && lastFilename_ == filename)
			return lastFileId_;

		auto it = filePathIds_.find(filename);
		std::uint32_t id;
		if (it == filePathIds_.end())
		{
			id = static_cast<std::uint32_t>(filePaths_.size());
			filePaths_.push_back(filename);
			filePathIds_.emplace(filename, id);
		}
		else
		{
			id = it->second;
		}

		hasLastFile_ = true;
		lastFilename_ = filename;
		lastFileId_ = id;
		return id;
	}

	//-------------------------------------------------------------------------
	std::uint32_t ExecutedAddressManager::GetOrCreateProbe(
	    std::uint32_t moduleId,
	    std::uint32_t fileId,
	    std::uint32_t lineNumber)
	{
		ProbeKey key{moduleId, fileId, lineNumber};
		auto it = probeIdByKey_.find(key);
		if (it != probeIdByKey_.end())
			return it->second;

		auto id = static_cast<std::uint32_t>(probeExecuted_.size());
		probeModuleId_.push_back(moduleId);
		probeFileId_.push_back(fileId);
		probeLineNumber_.push_back(lineNumber);
		probeExecuted_.push_back(0);
		probeIdByKey_.emplace(key, id);
		return id;
	}

	//-------------------------------------------------------------------------
	bool ExecutedAddressManager::RegisterAddress(
		const Address& address,
		const std::wstring& filename,
		std::uint32_t lineNumber,
		std::uint8_t instructionValue)
	{
		if (!hasLastModule_)
			THROW("Cannot get last module.");

		// Different {filename, line} can have the same address.
		// Same {filename, line} can have several addresses.
		auto fileId = InternFile(filename);
		auto probeId = GetOrCreateProbe(lastModuleId_, fileId, lineNumber);

		bool keepBreakpoint = false;
		auto itAddress = addressMap_.find(address);
		if (itAddress == addressMap_.end())
		{
			itAddress = addressMap_
			                .emplace(address,
			                         AddressData{instructionValue,
			                                     lastBaseOfImage_})
			                .first;
			keepBreakpoint = true;
		}

		itAddress->second.probes_.push_back(probeId);
		return keepBreakpoint;
	}

	//-------------------------------------------------------------------------
	std::optional<std::uint8_t> ExecutedAddressManager::MarkAddressAsExecuted(
		const Address& address)
	{
		auto it = addressMap_.find(address);
		if (it == addressMap_.end())
			return std::nullopt;

		for (auto probeId : it->second.probes_)
			probeExecuted_[probeId] = 1;
		return it->second.instructionToRestore_;
	}

	//-------------------------------------------------------------------------
	std::optional<std::uint8_t>
	ExecutedAddressManager::GetOriginalInstruction(const Address& address) const
	{
		auto it = addressMap_.find(address);
		if (it == addressMap_.end())
			return std::nullopt;
		return it->second.instructionToRestore_;
	}

	//-------------------------------------------------------------------------
	Plugin::CoverageData ExecutedAddressManager::CreateCoverageData(
		const std::wstring& name,
		std::int32_t exitCode) const
	{
		Plugin::CoverageData coverageData{ name, exitCode };

		// Group the probe columns by module and file, with lines sorted, in a
		// single pass. This runs once at the end of a coverage run, so the
		// temporary grouping is not on any hot path.
		//   moduleId -> fileId -> (lineNumber -> executed)
		std::unordered_map<
		    std::uint32_t,
		    std::unordered_map<std::uint32_t, std::map<std::uint32_t, bool>>>
		    linesByModuleFile;

		for (std::size_t probe = 0; probe < probeExecuted_.size(); ++probe)
		{
			linesByModuleFile[probeModuleId_[probe]][probeFileId_[probe]]
			                 [probeLineNumber_[probe]] =
			                     probeExecuted_[probe] != 0;
		}

		// Emit modules in name order (matching the previous std::map order),
		// including modules that registered no lines, and files in path
		// order so the output does not depend on registration order.
		for (const auto& [moduleName, moduleId] : moduleIdByName_)
		{
			auto& moduleCoverage = coverageData.AddModule(moduleName);

			auto moduleIt = linesByModuleFile.find(moduleId);
			if (moduleIt == linesByModuleFile.end())
				continue;

			std::vector<std::uint32_t> fileIds;
			fileIds.reserve(moduleIt->second.size());
			for (const auto& [fileId, lines] : moduleIt->second)
				fileIds.push_back(fileId);
			std::sort(fileIds.begin(),
			          fileIds.end(),
			          [&](std::uint32_t lhs, std::uint32_t rhs) {
				          return filePaths_[lhs] < filePaths_[rhs];
			          });

			for (auto fileId : fileIds)
			{
				auto& fileCoverage =
				    moduleCoverage.AddFile(filePaths_[fileId]);
				for (const auto& [lineNumber, executed] :
				     moduleIt->second[fileId])
					fileCoverage.AddLine(lineNumber, executed);
			}
		}

		return coverageData;
	}

	//-------------------------------------------------------------------------
	template <typename Condition>
	void ExecutedAddressManager::RemoveAddressIf(Condition condition)
	{
		auto it = addressMap_.begin();
		while (it != addressMap_.end())
		{
			if (condition(*it))
				it = addressMap_.erase(it);
			else
				++it;
		}
	}

	//-------------------------------------------------------------------------
	void ExecutedAddressManager::OnExitProcess(HANDLE hProcess)
	{
		// Only the per-process address fan-out is dropped; the probe table and
		// its executed bits persist so coverage aggregates across processes.
		RemoveAddressIf([=](const auto& pair) {
			return pair.first.GetProcessHandle() == hProcess;
		});
	}

	//-------------------------------------------------------------------------
	void ExecutedAddressManager::OnUnloadModule(HANDLE hProcess, void* dllBaseOfImage)
	{
		RemoveAddressIf([=](const auto& pair) {
			return pair.first.GetProcessHandle() == hProcess &&
			       pair.second.dllBaseOfImage_ == dllBaseOfImage;
		});
	}
}
