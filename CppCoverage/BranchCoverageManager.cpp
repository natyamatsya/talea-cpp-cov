// OpenCppCoverage is an open source code coverage for C++.
// Copyright (C) 2026 OpenCppCoverage
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
#include "BranchCoverageManager.hpp"

#include <unordered_map>

#include "tools/Log.hpp"

#include "CppCoverageException.hpp"
#include "Plugin/Exporter/ModuleCoverage.hpp"
#include "Plugin/Exporter/FileCoverage.hpp"
#include "Plugin/Exporter/BranchCoverage.hpp"
#include "Address.hpp"
#include "BreakPoint.hpp"
#include "BranchCondition.hpp"

namespace CppCoverage
{
	//-------------------------------------------------------------------------
	struct BranchCoverageManager::BranchOutcome
	{
		bool taken_ = false;
		bool fallThrough_ = false;
	};

	//-------------------------------------------------------------------------
	struct BranchCoverageManager::BranchState
	{
		BranchState(unsigned char originalInstruction,
		            void* dllBaseOfImage,
		            BranchCondition condition,
		            BranchOutcome* outcome)
			: originalInstruction_{originalInstruction}
			, dllBaseOfImage_{dllBaseOfImage}
			, condition_{condition}
			, outcome_{outcome}
		{
		}

		const unsigned char originalInstruction_;
		void* const dllBaseOfImage_;
		const BranchCondition condition_;
		BranchOutcome* const outcome_;
		bool armed_ = true;
	};

	//-------------------------------------------------------------------------
	struct BranchCoverageManager::File
	{
		// Use map to have stable pointers on values.
		// lineNumber -> branchIndex -> outcome
		std::map<unsigned int, std::map<unsigned int, BranchOutcome>>
		    branchesByLine_;
	};

	//-------------------------------------------------------------------------
	struct BranchCoverageManager::Module
	{
		explicit Module(const std::wstring& name) : name_{name}
		{
		}

		const std::wstring name_;
		std::unordered_map<std::wstring, File> files_;
	};

	//-------------------------------------------------------------------------
	BranchCoverageManager::BranchCoverageManager()
	{
		lastModule_.module_ = nullptr;
		lastModule_.baseOfImage_ = nullptr;
	}

	//-------------------------------------------------------------------------
	BranchCoverageManager::~BranchCoverageManager() = default;

	//-------------------------------------------------------------------------
	void BranchCoverageManager::AddModule(const std::wstring& moduleName,
	                                      void* dllBaseOfImage)
	{
		auto it = modules_.find(moduleName);

		if (it == modules_.end())
			it = modules_.emplace(moduleName, Module{moduleName}).first;
		lastModule_.module_ = &it->second;
		lastModule_.baseOfImage_ = dllBaseOfImage;
	}

	//-------------------------------------------------------------------------
	BranchCoverageManager::Module& BranchCoverageManager::GetLastAddedModule()
	{
		if (!lastModule_.module_)
			THROW("Cannot get last module.");

		return *lastModule_.module_;
	}

	//-------------------------------------------------------------------------
	void BranchCoverageManager::RegisterBranch(
	    const Address& address,
	    const std::wstring& filename,
	    unsigned int lineNumber,
	    BranchCondition condition,
	    unsigned char originalInstruction)
	{
		if (branches_.count(address) != 0)
			THROW("Branch is already registered.");

		auto& module = GetLastAddedModule();
		auto& file = module.files_[filename];
		auto& branches = file.branchesByLine_[lineNumber];

		auto branchIndex = static_cast<unsigned int>(branches.size());
		auto& outcome = branches[branchIndex];

		branches_.emplace(address,
		                  BranchState{originalInstruction,
		                              lastModule_.baseOfImage_,
		                              condition,
		                              &outcome});
	}

	//-------------------------------------------------------------------------
	bool BranchCoverageManager::IsMonitoredBranch(const Address& address) const
	{
		return branches_.find(address) != branches_.end();
	}

	//-------------------------------------------------------------------------
	std::optional<unsigned char>
	BranchCoverageManager::GetOriginalInstruction(const Address& address) const
	{
		auto it = branches_.find(address);

		if (it == branches_.end())
			return std::nullopt;
		return it->second.originalInstruction_;
	}

	//-------------------------------------------------------------------------
	void BranchCoverageManager::OnBreakPointHit(const Address& address,
	                                            HANDLE hThread,
	                                            const BreakPoint& breakPoint)
	{
		auto it = branches_.find(address);

		if (it == branches_.end())
			THROW("Address is not a monitored branch.");

		auto& state = it->second;

		CONTEXT context;
		context.ContextFlags = CONTEXT_CONTROL | CONTEXT_INTEGER;
		if (!GetThreadContext(hThread, &context))
			THROW_LAST_ERROR("Error in GetThreadContext", GetLastError());

		DWORD64 rcx = context.Rcx;
		if (IsBranchTaken(state.condition_, context.EFlags, rcx))
			state.outcome_->taken_ = true;
		else
			state.outcome_->fallThrough_ = true;

		if (state.outcome_->taken_ && state.outcome_->fallThrough_)
			state.armed_ = false;

		breakPoint.RemoveBreakPoint(address, state.originalInstruction_);
		breakPoint.AdjustIpAndSetTrapFlag(hThread);

		auto threadId = GetThreadId(hThread);
		pendingReArms_[threadId] = std::make_pair(
		    address.GetProcessHandle(), address.GetValue());
	}

	//-------------------------------------------------------------------------
	bool BranchCoverageManager::OnSingleStep(HANDLE hProcess,
	                                         HANDLE hThread,
	                                         const BreakPoint& breakPoint)
	{
		auto threadId = GetThreadId(hThread);
		auto it = pendingReArms_.find(threadId);

		if (it == pendingReArms_.end() || it->second.first != hProcess)
			return false;

		Address address{it->second.first, it->second.second};
		pendingReArms_.erase(it);

		auto itBranch = branches_.find(address);
		if (itBranch != branches_.end() && itBranch->second.armed_)
			breakPoint.SetBreakPoint(address);

		return true;
	}

	//-------------------------------------------------------------------------
	void BranchCoverageManager::OnExitProcess(HANDLE hProcess)
	{
		auto it = branches_.begin();
		while (it != branches_.end())
		{
			if (it->first.GetProcessHandle() == hProcess)
				it = branches_.erase(it);
			else
				++it;
		}

		auto itPending = pendingReArms_.begin();
		while (itPending != pendingReArms_.end())
		{
			if (itPending->second.first == hProcess)
				itPending = pendingReArms_.erase(itPending);
			else
				++itPending;
		}
	}

	//-------------------------------------------------------------------------
	void BranchCoverageManager::OnExitThread(DWORD threadId)
	{
		// A thread that exits mid-cycle (after a branch hit but before its
		// single-step) leaves a stale entry; drop it so a later thread reusing
		// the id is not mistaken for a pending re-arm.
		pendingReArms_.erase(threadId);
	}

	//-------------------------------------------------------------------------
	void BranchCoverageManager::OnUnloadModule(HANDLE hProcess,
	                                           void* dllBaseOfImage)
	{
		auto it = branches_.begin();
		while (it != branches_.end())
		{
			if (it->first.GetProcessHandle() == hProcess &&
			    it->second.dllBaseOfImage_ == dllBaseOfImage)
			{
				it = branches_.erase(it);
			}
			else
				++it;
		}
	}

	//-------------------------------------------------------------------------
	void BranchCoverageManager::DecorateCoverageData(
	    Plugin::CoverageData& coverageData) const
	{
		for (const auto& moduleCoverage : coverageData.GetModules())
		{
			auto itModule = modules_.find(moduleCoverage->GetPath().wstring());
			if (itModule == modules_.end())
				continue;
			const auto& module = itModule->second;

			for (const auto& fileCoverage : moduleCoverage->GetFiles())
			{
				auto itFile =
				    module.files_.find(fileCoverage->GetPath().wstring());
				if (itFile == module.files_.end())
					continue;
				const auto& file = itFile->second;

				for (const auto& linePair : file.branchesByLine_)
				{
					auto lineNumber = linePair.first;

					for (const auto& branchPair : linePair.second)
					{
						const auto& outcome = branchPair.second;
						fileCoverage->AddBranch(
						    lineNumber,
						    Plugin::BranchCoverage{branchPair.first,
						                           outcome.taken_,
						                           outcome.fallThrough_});
					}
				}
			}
		}
	}
}
