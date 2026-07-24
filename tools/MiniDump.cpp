// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 OpenCppCoverage

#include "stdafx.h"
#include "MiniDump.hpp"

#include <iostream>

#include <cstdlib>
#include <crtdbg.h>
#include <werapi.h>

#pragma warning(push)
#pragma warning(disable: 4091) // 'typedef ': ignored on left of '' when no variable is declared
#include <DbgHelp.h>
#pragma warning(pop)

namespace Tools
{
	namespace
	{

		//-----------------------------------------------------------------------------
		MINIDUMP_TYPE GetMiniDumpDefaultType()
		{
			return static_cast<MINIDUMP_TYPE>(MiniDumpWithDataSegs |
				MiniDumpWithPrivateReadWriteMemory |
				MiniDumpWithFullMemoryInfo |
				MiniDumpWithThreadInfo);
		}

		//-----------------------------------------------------------------------------
		void CreateMiniDump(
			MINIDUMP_EXCEPTION_INFORMATION& minidumpInfo, 
			HANDLE hFile, 			
			const wchar_t* dmpFilename)
		{
			auto miniDumpType = GetMiniDumpDefaultType();

			std::wcerr << L"\tTrying to create memory dump..." << std::endl;
			if (MiniDumpWriteDump(
				GetCurrentProcess(),
				GetCurrentProcessId(),
				hFile,
				miniDumpType,
				&minidumpInfo,
				nullptr,
				nullptr))
			{
				std::wcerr << "\tMemory dump created successfully: " << dmpFilename << std::endl;
				std::wcerr << "\tPlease create a new issue on ";
				std::wcerr << "https://github.com/OpenCppCoverage/OpenCppCoverage/issues and attached the memory dump ";
				std::wcerr << dmpFilename << std::endl;
			}
			else
				std::cerr << "\tFailed to create memory dump." << std::endl;
		}
		
		//-----------------------------------------------------------------------------
		LONG WINAPI CreateMiniDumpOnUnHandledException(PEXCEPTION_POINTERS exceptionInfo)
		{
			MINIDUMP_EXCEPTION_INFORMATION minidumpInfo;

			std::wcerr << L"Unexpected error occurs." << std::endl;			

			minidumpInfo.ThreadId = GetCurrentThreadId();
			minidumpInfo.ExceptionPointers = exceptionInfo;
			minidumpInfo.ClientPointers = FALSE;

			const auto dmpFilename = L"TaleaCppCovApp.dmp";
			HANDLE hFile = CreateFile(dmpFilename, GENERIC_WRITE, 0, nullptr,
				CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
			
			if (hFile != INVALID_HANDLE_VALUE)
			{				
				CreateMiniDump(minidumpInfo, hFile, dmpFilename);
				CloseHandle(hFile);
			}
			abort();
			return 0;
		}	
	}

	//-------------------------------------------------------------------------
	void CreateMiniDumpOnUnHandledException()
	{
		DWORD dwMode = GetErrorMode();
		SetErrorMode(dwMode | SEM_NOGPFAULTERRORBOX);

		SetUnhandledExceptionFilter(CreateMiniDumpOnUnHandledException);
	}

	//-------------------------------------------------------------------------
	void DisableCrashDialogs()
	{
		// (1) The OS-level hard-error / GP-fault boxes. Inherited by children.
		SetErrorMode(GetErrorMode() | SEM_FAILCRITICALERRORS |
		             SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);

		// (2) The modern Windows Error Reporting UI ("… has stopped working")
		// for this process, which SetErrorMode alone does not suppress.
		WerSetFlags(WER_FAULT_REPORTING_NO_UI);

		// (3) The C runtime's own dialogs. An uncaught exception ends in abort(),
		// which in a debug build pops the "abort() has been called" message box;
		// clear its behaviours and route the debug report to stderr instead of a
		// window. These are per-process CRT state and are NOT inherited, so every
		// process that can crash must call this itself.
		_set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#ifdef _DEBUG
		for (int report : {_CRT_ERROR, _CRT_ASSERT})
		{
			_CrtSetReportMode(report, _CRTDBG_MODE_FILE);
			_CrtSetReportFile(report, _CRTDBG_FILE_STDERR);
		}
#endif
	}
}