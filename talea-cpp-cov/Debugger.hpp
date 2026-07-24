// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2014 OpenCppCoverage

#pragma once
#include <cstdint>

#include <optional>

#include <unordered_map>
#include <Windows.h>
#include "TaleaCppCovExport.hpp"

#include "tools/Error.hpp"


namespace TaleaCppCov
{
	class StartInfo;
	class IDebugEventsHandler;

	class TALEACPPCOV_DLL Debugger
	{
	public:
		Debugger(
			bool coverChildren,
			bool continueAfterCppException,
            bool stopOnAssert);

		Tools::Expected<std::int32_t> Debug(const StartInfo&, IDebugEventsHandler&);
		size_t GetRunningProcesses() const;
		size_t GetRunningThreads() const;

	private:
		Debugger(const Debugger&) = delete;
		Debugger& operator=(const Debugger&) = delete;

		Tools::Expected<void> OnCreateProcess(
			const DEBUG_EVENT& debugEvent,
			IDebugEventsHandler& debugEventsHandler);

		Tools::Expected<std::int32_t> OnExitProcess(
			const DEBUG_EVENT& debugEvent,
			HANDLE hProcess,
			HANDLE hThread,
			IDebugEventsHandler& debugEventsHandler);

		Tools::Expected<void> OnCreateThread(
			HANDLE hThread,
			DWORD dwThreadId);

		Tools::Expected<void> OnExitThread(DWORD dwProcessId);

		HANDLE GetProcessHandle(DWORD dwProcessId) const;
		HANDLE GetThreadHandle(DWORD dwThreadId) const;

		struct ProcessStatus;

		Tools::Expected<ProcessStatus> HandleDebugEvent(const DEBUG_EVENT&, IDebugEventsHandler&);

		Tools::Expected<ProcessStatus> HandleNotCreationalEvent(
			const DEBUG_EVENT& debugEvent,
			IDebugEventsHandler& debugEventsHandler,
			HANDLE hProcess,
			HANDLE hThread,
			DWORD dwThreadId);

		ProcessStatus OnException(const DEBUG_EVENT&, IDebugEventsHandler&, HANDLE hProcess, HANDLE hThread) const;

	private:
		std::unordered_map<DWORD, HANDLE> processHandles_;
		std::unordered_map<DWORD, HANDLE> threadHandles_;
		std::optional<DWORD> rootProcessId_;
		bool coverChildren_;
		bool continueAfterCppException_;
        bool stopOnAssert_;
    };
}


