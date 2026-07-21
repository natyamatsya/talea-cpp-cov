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
#include "CodeCoverageRunner.hpp"

#include <sstream>
#include <optional>

#include "tools/Log.hpp"

#include "Plugin/Exporter/CoverageData.hpp"
#include "Debugger.hpp"
#include "ExecutedAddressManager.hpp"
#include "BranchCoverageManager.hpp"
#include "HandleInformation.hpp"
#include "BreakPoint.hpp"
#include "CoverageFilterManager.hpp"
#include "StartInfo.hpp"
#include "ExceptionHandler.hpp"
#include "CppCoverageException.hpp"
#include "Address.hpp"
#include "RunCoverageSettings.hpp"
#include "MonitoredLineRegister.hpp"
#include "FilterAssistant.hpp"
#include "FileSystem.hpp"
#include "PdbLineTableCache.hpp"

#include "Tools/WarningManager.hpp"
#include "Tools/Tool.hpp"

namespace CppCoverage
{
	//-------------------------------------------------------------------------
	CodeCoverageRunner::CodeCoverageRunner(
	    std::shared_ptr<Tools::WarningManager> warningManager)
	    : warningManager_{warningManager},
	      filterAssistant_{
	          std::make_shared<FilterAssistant>(std::make_shared<FileSystem>())}
	{
		executedAddressManager_ = std::make_shared<ExecutedAddressManager>();
		exceptionHandler_ = std::make_unique<ExceptionHandler>();
		breakpoint_ = std::make_shared<BreakPoint>();
	}
	
	//-------------------------------------------------------------------------
	CodeCoverageRunner::~CodeCoverageRunner()
	{
	}

	//-------------------------------------------------------------------------
	Plugin::CoverageData CodeCoverageRunner::RunCoverage(
		const RunCoverageSettings& settings)
	{
		Debugger debugger{ settings.GetCoverChildren(), settings.GetContinueAfterCppException(), settings.GetStopOnAssert()};

		coverageFilterManager_ = std::make_shared<CoverageFilterManager>(
			settings.GetCoverageFilterSettings(),
			settings.GetUnifiedDiffSettings(),
			settings.GetExcludedLineRegexes(),
			settings.GetOptimizedBuildSupport());

		if (settings.GetBranchCoverage())
			branchCoverageManager_ = std::make_shared<BranchCoverageManager>();

		std::shared_ptr<PdbLineTableCache> pdbLineTableCache;
		if (settings.IsPdbCacheEnabled())
		{
			pdbLineTableCache = std::make_shared<PdbLineTableCache>(
			    PdbLineTableCache::GetDefaultDiskCacheDirectory());
		}

		monitoredLineRegister_ = std::make_unique<MonitoredLineRegister>(
		    breakpoint_,
		    executedAddressManager_,
		    coverageFilterManager_,
		    std::make_unique<DebugInformationEnumerator>(
		        settings.GetSubstitutePdbSourcePaths(),
		        settings.GetOptimizedBuildSupport(),
		        pdbLineTableCache),
			filterAssistant_,
			branchCoverageManager_);

		const auto& startInfo = settings.GetStartInfo();
		int exitCode = debugger.Debug(startInfo, *this);
		const auto& path = startInfo.GetPath();

		if (pdbLineTableCache)
		{
			LOG_DEBUG << L"PDB line-table cache: "
			          << pdbLineTableCache->GetMemoryHitCount()
			          << L" memory hit(s), "
			          << pdbLineTableCache->GetDiskHitCount()
			          << L" disk hit(s), "
			          << pdbLineTableCache->GetMissCount() << L" miss(es).";
		}

		auto warningMessageLines = coverageFilterManager_->ComputeWarningMessageLines(
			settings.GetMaxUnmatchPathsForWarning());
		for (const auto& line : warningMessageLines)
				LOG_WARNING << line;
		auto filterAdviceMessage = filterAssistant_->GetAdviceMessage();
		if (filterAdviceMessage)
			warningManager_->AddWarning(*filterAdviceMessage);
		auto coverageData = executedAddressManager_->CreateCoverageData(
		    path.filename().wstring(), exitCode);
		if (branchCoverageManager_)
			branchCoverageManager_->DecorateCoverageData(coverageData);
		return coverageData;
	}

	//-------------------------------------------------------------------------
	void CodeCoverageRunner::OnCreateProcess(const CREATE_PROCESS_DEBUG_INFO& processDebugInfo)
	{
		auto hProcess = processDebugInfo.hProcess;
		auto lpBaseOfImage = processDebugInfo.lpBaseOfImage;

		LoadModule(hProcess, processDebugInfo.hFile, lpBaseOfImage);
	}
	
	//-------------------------------------------------------------------------
	void CodeCoverageRunner::OnExitProcess(HANDLE hProcess, HANDLE, const EXIT_PROCESS_DEBUG_INFO&)
	{
		exceptionHandler_->OnExitProcess(hProcess);
		executedAddressManager_->OnExitProcess(hProcess);
		if (branchCoverageManager_)
			branchCoverageManager_->OnExitProcess(hProcess);
	}

	//-------------------------------------------------------------------------
	void CodeCoverageRunner::OnLoadDll(
		HANDLE hProcess, 
		HANDLE hThread, 
		const LOAD_DLL_DEBUG_INFO& dllDebugInfo)
	{
		LoadModule(hProcess, dllDebugInfo.hFile, dllDebugInfo.lpBaseOfDll);
	}
	
	//-------------------------------------------------------------------------
	void CodeCoverageRunner::OnUnloadDll(
		HANDLE hProcess,
		HANDLE hThread,
		const UNLOAD_DLL_DEBUG_INFO& unloadDllDebugInfo)
	{
		executedAddressManager_->OnUnloadModule(hProcess, unloadDllDebugInfo.lpBaseOfDll);
		if (branchCoverageManager_)
			branchCoverageManager_->OnUnloadModule(
			    hProcess, unloadDllDebugInfo.lpBaseOfDll);
	}

	//-------------------------------------------------------------------------
	IDebugEventsHandler::ExceptionType CodeCoverageRunner::OnException(
		HANDLE hProcess, 
		HANDLE hThread, 
		const EXCEPTION_DEBUG_INFO& exceptionDebugInfo)
	{
		// Reuse the stream across events instead of constructing a fresh
		// wostringstream (and re-imbuing the locale) on every debug event.
		exceptionMessage_.str(std::wstring{});
		exceptionMessage_.clear();

		auto status = exceptionHandler_->HandleException(
		    hProcess, exceptionDebugInfo, exceptionMessage_);

		switch (status)
		{
			case CppCoverage::ExceptionHandlerStatus::BreakPoint:
			{
				if (OnBreakPoint(exceptionDebugInfo, hProcess, hThread))
					return IDebugEventsHandler::ExceptionType::BreakPoint;
				return IDebugEventsHandler::ExceptionType::InvalidBreakPoint;
			}
			case CppCoverage::ExceptionHandlerStatus::SingleStep:
			{
				if (branchCoverageManager_ &&
				    branchCoverageManager_->OnSingleStep(
				        hProcess, hThread, *breakpoint_))
				{
					return IDebugEventsHandler::ExceptionType::BreakPoint;
				}
				return IDebugEventsHandler::ExceptionType::NotHandled;
			}
			case CppCoverage::ExceptionHandlerStatus::FirstChanceException:
			{
				return IDebugEventsHandler::ExceptionType::NotHandled;
			}
			case CppCoverage::ExceptionHandlerStatus::Error:
			{
				LOG_ERROR << exceptionMessage_.str();

				return IDebugEventsHandler::ExceptionType::Error;
			}
			case CppCoverage::ExceptionHandlerStatus::CppError:
			{
				LOG_ERROR << exceptionMessage_.str();

				return IDebugEventsHandler::ExceptionType::CppError;
			}
		}

		return IDebugEventsHandler::ExceptionType::NotHandled;
	}
	
	//-------------------------------------------------------------------------
	bool CodeCoverageRunner::OnBreakPoint(
		const EXCEPTION_DEBUG_INFO& exceptionDebugInfo,
		HANDLE hProcess,
		HANDLE hThread)
	{
		const auto& exceptionRecord = exceptionDebugInfo.ExceptionRecord;
		auto addressValue = exceptionRecord.ExceptionAddress;
		Address address{ hProcess, addressValue };

		if (branchCoverageManager_ &&
		    branchCoverageManager_->IsMonitoredBranch(address))
		{
			// The same address can also be the start of a line:
			// mark it as executed but keep the branch breakpoint
			// life cycle (restore, single-step and set again).
			executedAddressManager_->MarkAddressAsExecuted(address);
			branchCoverageManager_->OnBreakPointHit(
			    address, hThread, *breakpoint_);
			return true;
		}

		auto oldInstruction = executedAddressManager_->MarkAddressAsExecuted(address);

		if (oldInstruction)
		{
			breakpoint_->RemoveBreakPoint(address, *oldInstruction);
			breakpoint_->AdjustEipAfterBreakPointRemoval(hThread);
			return true;
		}

		return false;
	}

	//-------------------------------------------------------------------------
	void CodeCoverageRunner::LoadModule(HANDLE hProcess,
	                                    HANDLE hFile,
	                                    void* baseOfImage)
	{
		HandleInformation handleInformation;

		std::wstring filename = handleInformation.ComputeFilename(hFile);

		auto isSelected = coverageFilterManager_->IsModuleSelected(filename);
		if (isSelected)
		{
			isSelected = monitoredLineRegister_->RegisterLineToMonitor(
			    filename, hProcess, baseOfImage);
		}
		filterAssistant_->OnNewModule(filename, isSelected);
	}
}
