// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2014 OpenCppCoverage

#include "stdafx.h"
#include "IDebugEventsHandler.hpp"

namespace TaleaCppCov
{
	//-------------------------------------------------------------------------
	void IDebugEventsHandler::OnCreateProcess(const CREATE_PROCESS_DEBUG_INFO&) 
	{
	}

	//-------------------------------------------------------------------------
	void IDebugEventsHandler::OnExitProcess(HANDLE hProcess, HANDLE hThread, const EXIT_PROCESS_DEBUG_INFO&)
	{
	}

	//-------------------------------------------------------------------------
	void IDebugEventsHandler::OnLoadDll(HANDLE hProcess, HANDLE hThread, const LOAD_DLL_DEBUG_INFO&)
	{
	}

	//-------------------------------------------------------------------------
	void IDebugEventsHandler::OnUnloadDll(HANDLE hProcess, HANDLE hThread, const UNLOAD_DLL_DEBUG_INFO&)
	{
	}

	//-------------------------------------------------------------------------
	void IDebugEventsHandler::OnExitThread(DWORD)
	{
	}

	//-------------------------------------------------------------------------
	IDebugEventsHandler::ExceptionType IDebugEventsHandler::OnException(HANDLE hProcess, HANDLE hThread, const EXCEPTION_DEBUG_INFO&)
	{ 
		return IDebugEventsHandler::ExceptionType::NotHandled;
	}
}
