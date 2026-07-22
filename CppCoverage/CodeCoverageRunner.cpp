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
#include "CodeCoverageRunner.hpp"

#include <algorithm>
#include <execution>
#include <sstream>
#include <optional>

#include "Tools/Log.hpp"

#include "Plugin/Exporter/CoverageData.hpp"
#include "Debugger.hpp"
#include "ImportClosure.hpp"
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
	namespace
	{
		// Warms the PDB line-table cache by parsing a module without replaying
		// anything: the cached table is the full, pre-filter table, so any
		// handler works.
		class PreparseHandler : public IDebugInformationHandler
		{
		  public:
			bool IsSourceFileSelected(const std::filesystem::path&) override
			{
				return true;
			}
			void OnSourceFile(const std::filesystem::path&,
			                  const std::vector<Line>&) override
			{
			}
		};

		//---------------------------------------------------------------------
		// Parses the executable and its import-closure DLLs in parallel to warm
		// the PDB line-table cache before the program runs, so each module loads
		// without a per-module parse stalling the debug loop. Best-effort: a
		// module that fails to pre-parse is simply parsed on load as usual.
		void PreparseImportClosure(const StartInfo& startInfo,
		                           const RunCoverageSettings& settings,
		                           CoverageFilterManager& filterManager,
		                           std::shared_ptr<PdbLineTableCache> cache)
		{
			std::vector<std::filesystem::path> modules{startInfo.GetPath()};
			for (const auto& module :
			     ComputeImportClosure(startInfo.GetPath()))
			{
				if (filterManager.IsModuleSelected(module.wstring()))
					modules.push_back(module);
			}

			const auto& substitutePaths = settings.GetSubstitutePdbSourcePaths();
			std::for_each(
			    std::execution::par,
			    modules.begin(),
			    modules.end(),
			    [&](const std::filesystem::path& modulePath) {
				    try
				    {
					    DebugInformationEnumerator enumerator{
					        substitutePaths,
					        /*needSymbolIndex*/ false,
					        cache,
					        /*needFunctionNames*/ false,
					        /*useRawPdb*/ true,
					        /*rawPdbOnly*/ true};
					    PreparseHandler handler;
					    enumerator.Enumerate(modulePath, handler);
				    }
				    catch (...)
				    {
					    // Best-effort: the on-load parse handles this module.
				    }
			    });
		}
	}

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

		// Run merging relies on the debug-build property that a line's code
		// range is contiguous; optimized builds fall back to per-line
		// breakpoints.
		auto enableLineMerging = settings.IsLineMergingEnabled() &&
		                         !settings.GetOptimizedBuildSupport();

		monitoredLineRegister_ = std::make_unique<MonitoredLineRegister>(
		    breakpoint_,
		    executedAddressManager_,
		    coverageFilterManager_,
		    std::make_unique<DebugInformationEnumerator>(
		        settings.GetSubstitutePdbSourcePaths(),
		        settings.GetOptimizedBuildSupport(),
		        pdbLineTableCache,
		        /*needFunctionNames*/ false,
		        settings.IsRawPdbEnabled()),
			filterAssistant_,
			branchCoverageManager_,
			enableLineMerging);

		const auto& startInfo = settings.GetStartInfo();

		// Optional: parse the import closure's PDBs in parallel up front so the
		// module loads below hit the cache. Restricted to the raw_pdb path (the
		// DIA path is COM-apartment-bound and hostile to parallelism, and the
		// optimized-build filter needs symbol indexes the raw reader skips).
		if (settings.GetPreparseImports() && pdbLineTableCache &&
		    settings.IsRawPdbEnabled() && !settings.GetOptimizedBuildSupport())
		{
			PreparseImportClosure(
			    startInfo, settings, *coverageFilterManager_, pdbLineTableCache);
		}

		std::int32_t exitCode = debugger.Debug(startInfo, *this);
		const auto& path = startInfo.GetPath();

		if (pdbLineTableCache)
		{
			LOG_DEBUG("PDB line-table cache: {} memory hit(s), {} disk hit(s), {} miss(es).", pdbLineTableCache->GetMemoryHitCount(), pdbLineTableCache->GetDiskHitCount(), pdbLineTableCache->GetMissCount());
		}

		auto warningMessageLines = coverageFilterManager_->ComputeWarningMessageLines(
			settings.GetMaxUnmatchPathsForWarning());
		for (const auto& line : warningMessageLines)
				LOG_WARNING("{}", line);
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
	void CodeCoverageRunner::OnExitThread(DWORD threadId)
	{
		// Drop any pending branch single-step re-arm owned by the exiting
		// thread so a later thread reusing the id cannot be mistaken for it.
		if (branchCoverageManager_)
			branchCoverageManager_->OnExitThread(threadId);
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
				LOG_ERROR("{}", exceptionMessage_.str());

				return IDebugEventsHandler::ExceptionType::Error;
			}
			case CppCoverage::ExceptionHandlerStatus::CppError:
			{
				LOG_ERROR("{}", exceptionMessage_.str());

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
			breakpoint_->AdjustIpAfterBreakPointRemoval(hThread);
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
