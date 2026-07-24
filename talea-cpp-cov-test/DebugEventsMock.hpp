// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2014 OpenCppCoverage

#pragma once

#include "talea-cpp-cov/IDebugEventsHandler.hpp"

namespace TaleaCppCovTest
{
	class DebugEventsHandlerMock : public TaleaCppCov::IDebugEventsHandler
	{
	public:
		DebugEventsHandlerMock() = default;

		MOCK_METHOD1(OnCreateProcess, void(const CREATE_PROCESS_DEBUG_INFO&));
		MOCK_METHOD3(OnExitProcess, void(HANDLE, HANDLE, const EXIT_PROCESS_DEBUG_INFO&));
		MOCK_METHOD3(OnLoadDll, void(HANDLE, HANDLE, const LOAD_DLL_DEBUG_INFO&));
		MOCK_METHOD3(OnUnloadDll, void(HANDLE, HANDLE, const UNLOAD_DLL_DEBUG_INFO&));
		MOCK_METHOD3(OnException, ExceptionType(HANDLE, HANDLE, const EXCEPTION_DEBUG_INFO&));

	private:
		DebugEventsHandlerMock(const DebugEventsHandlerMock&) = delete;
		DebugEventsHandlerMock& operator=(const DebugEventsHandlerMock&) = delete;
	};
}