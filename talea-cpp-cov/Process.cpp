// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2014 OpenCppCoverage

#include "stdafx.h"
#include "Process.hpp"

#include <Windows.h>
#include <optional>

#include "tools/Log.hpp"
#include "tools/Tool.hpp"

#include "StartInfo.hpp"
#include "TaleaCppCovException.hpp" // TaleaCppCov::GetErrorMessage
#include "tools/Error.hpp"

namespace TaleaCppCov
{
	namespace
	{
		//---------------------------------------------------------------------
		std::optional<std::vector<wchar_t>> 
			CreateCommandLine(const std::vector<std::wstring>& arguments)
		{
			std::optional<std::vector<wchar_t>> commandLine;

			if (!arguments.empty())
			{				
				std::vector<wchar_t> buffer;
				for (const auto& argument : arguments)
				{
					buffer.push_back(L'\"');
					buffer.insert(buffer.end(), argument.begin(), argument.end());
					buffer.push_back(L'\"');
					buffer.push_back(L' ');
				}
					
				buffer.push_back(L'\0');
				return buffer;
			}

			return commandLine;
		}

		//---------------------------------------------------------------------
		// Builds an environment block inheriting the current environment plus
		// _NO_DEBUG_HEAP=1. A process created by a debugger otherwise uses the
		// slow NT debug heap (tail checking, fill patterns, serialized
		// validation), which makes allocation-heavy debuggees markedly slower.
		std::vector<wchar_t> CreateEnvironmentWithoutDebugHeap()
		{
			std::vector<wchar_t> block;

			auto* environment = GetEnvironmentStringsW();
			if (environment)
			{
				const wchar_t* entry = environment;
				while (*entry)
				{
					auto length = wcslen(entry);
					block.insert(block.end(), entry, entry + length + 1);
					entry += length + 1;
				}
				FreeEnvironmentStringsW(environment);
			}

			static const wchar_t noDebugHeap[] = L"_NO_DEBUG_HEAP=1";
			// std::end includes the terminating null of the string literal.
			block.insert(block.end(), std::begin(noDebugHeap), std::end(noDebugHeap));
			block.push_back(L'\0'); // final block terminator

			return block;
		}
	}

	const std::wstring Process::CannotFindPathMessage = L"Cannot find path: ";
	const std::wstring Process::CheckIfValidExecutableMessage =
	    L"Cannot run process, check if it is a valid executable:";

	//-------------------------------------------------------------------------
	Process::Process(const StartInfo& startInfo)
		: startInfo_(startInfo)
	{		
	}

	//-------------------------------------------------------------------------
	Process::~Process()
	{
		if (processInformation_)
		{			
			auto hProcess = processInformation_->hProcess;
			if (hProcess && !CloseHandle(hProcess))
				LOG_ERROR("Cannot close process handle");

			auto hThread = processInformation_->hThread;
			if (hThread && !CloseHandle(hThread))
				LOG_ERROR("Cannot close thread handle");
		}
	}

	//-------------------------------------------------------------------------
	Tools::Expected<void> Process::Start(DWORD creationFlags)
	{
		if (processInformation_)
			return std::unexpected(MAKE_ERROR(L"Process already started"));

		STARTUPINFO lpStartupInfo;

		ZeroMemory(&lpStartupInfo, sizeof(lpStartupInfo));
		const auto* workindDirectory = startInfo_.GetWorkingDirectory();
		auto optionalCommandLine = CreateCommandLine(startInfo_.GetArguments());
		auto commandLine = (optionalCommandLine) ? &(*optionalCommandLine)[0] : nullptr;
		auto environment = CreateEnvironmentWithoutDebugHeap();

		processInformation_ = PROCESS_INFORMATION{};
		if (!CreateProcess(
			nullptr,
			commandLine,
			nullptr,
			nullptr,
			FALSE,
			creationFlags | CREATE_UNICODE_ENVIRONMENT,
			environment.data(),
			(workindDirectory) ? workindDirectory->c_str() : nullptr,
			&lpStartupInfo,
			&*processInformation_
			))
		{
			std::wostringstream ostr;

			if (!Tools::FileExists(startInfo_.GetPath()))
				ostr << CannotFindPathMessage + startInfo_.GetPath().wstring();
			else
			{
				ostr
				    << CheckIfValidExecutableMessage
				    << std::endl;

				ostr << startInfo_
				     << TaleaCppCov::GetErrorMessage(GetLastError());
			}
			return std::unexpected(Tools::Error{ostr.str()});
		}
		return {};
	}
}
