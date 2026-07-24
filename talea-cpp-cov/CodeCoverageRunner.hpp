// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2014 OpenCppCoverage

#pragma once

#include <memory>
#include <sstream>

#include "coverage/CoverageData.hpp"
#include "IDebugEventsHandler.hpp"
#include "TaleaCppCovExport.hpp"

namespace Tools
{
	class WarningManager;
}

namespace TaleaCppCov
{
	class StartInfo;
	class RunCoverageSettings;
	class DebugInformationEventHandler;
	class ExecutedAddressManager;
	class BranchCoverageManager;
	class BreakPoint;
	class CoverageFilterManager;
	class ExceptionHandler;
	class UnifiedDiffSettings;
	class MonitoredLineRegister;
	class FilterAssistant;

	class TALEACPPCOV_DLL CodeCoverageRunner : private IDebugEventsHandler
	{
	public:
		explicit CodeCoverageRunner(std::shared_ptr<Tools::WarningManager>);
		~CodeCoverageRunner();

		Coverage::CoverageData RunCoverage(const RunCoverageSettings&);

	private:
		virtual void OnCreateProcess(const CREATE_PROCESS_DEBUG_INFO&) override;
		virtual void OnExitProcess(HANDLE hProcess, HANDLE hThread, const EXIT_PROCESS_DEBUG_INFO&) override;
		virtual void OnLoadDll(HANDLE hProcess, HANDLE hThread, const LOAD_DLL_DEBUG_INFO&) override;
		virtual void OnUnloadDll(HANDLE hProcess, HANDLE hThread, const UNLOAD_DLL_DEBUG_INFO&) override;
		virtual void OnExitThread(DWORD threadId) override;
		virtual ExceptionType OnException(HANDLE hProcess, HANDLE hThread, const EXCEPTION_DEBUG_INFO&) override;

	private:
		CodeCoverageRunner(const CodeCoverageRunner&) = delete;
		CodeCoverageRunner& operator=(const CodeCoverageRunner&) = delete;

		void LoadModule(HANDLE hProcess, HANDLE hFile, void* baseOfImage);
		bool OnBreakPoint(const EXCEPTION_DEBUG_INFO&, HANDLE hProcess, HANDLE hThread);

	private:
		std::shared_ptr<BreakPoint> breakpoint_;
		std::shared_ptr<ExecutedAddressManager> executedAddressManager_;
		std::shared_ptr<BranchCoverageManager> branchCoverageManager_;
		std::shared_ptr<CoverageFilterManager> coverageFilterManager_;
		std::unique_ptr<MonitoredLineRegister> monitoredLineRegister_;
		std::unique_ptr<ExceptionHandler> exceptionHandler_;
		std::shared_ptr<Tools::WarningManager> warningManager_;
		std::shared_ptr<FilterAssistant> filterAssistant_;
		std::wostringstream exceptionMessage_;
	};
}


