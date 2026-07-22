// OpenCppCoverage is an open source code coverage for C++.
// Copyright (C) 2016 OpenCppCoverage
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
#include "Tools.hpp"
#include "Tools/Tool.hpp"

#include <Windows.h>

#include <fstream>
#include <filesystem>
#include <vector>
#include "Tools/StringUtils.hpp"

namespace fs = std::filesystem;

namespace
{
	//-------------------------------------------------------------------------
	// Quotes an argument for a Windows command line (the standard MSVCRT
	// rules), so LaunchProcess can join args the way Poco::Process::launch did.
	std::string QuoteArgument(const std::string& argument)
	{
		if (!argument.empty() &&
		    argument.find_first_of(" \t\"") == std::string::npos)
			return argument;

		std::string result = "\"";
		for (std::size_t i = 0; i < argument.size();)
		{
			std::size_t backslashes = 0;
			while (i < argument.size() && argument[i] == '\\')
			{
				++backslashes;
				++i;
			}
			if (i == argument.size())
			{
				result.append(backslashes * 2, '\\');
				break;
			}
			if (argument[i] == '"')
			{
				result.append(backslashes * 2 + 1, '\\');
				result.push_back('"');
			}
			else
			{
				result.append(backslashes, '\\');
				result.push_back(argument[i]);
			}
			++i;
		}
		result.push_back('"');
		return result;
	}
}

namespace TestHelper
{
	//-------------------------------------------------------------------------
	fs::path GetTestUnloadDllFilename()
	{
		// The source file compiled into TestHelper.dll (TestUnloadHelper.cpp);
		// this is the file the coverage tool reports when TestUnloadDll runs.
		return L"TestUnloadHelper.cpp";
	}

	//-------------------------------------------------------------------------
	fs::path GetOutputBinaryPath()
	{
		// The runtime-loaded DLL is the separate TestUnloadHelper target, whose
		// output is TestHelper.dll, next to the test executable (flat bin dir).
		return fs::path(OUT_DIR) / L"TestHelper.dll";
	}

	//-------------------------------------------------------------------------
	void CreateEmptyFile(const fs::path& path)
	{
		std::ofstream ofs;
		ofs.open(path.string(), std::ios::out);
	}

	//-------------------------------------------------------------------------
	ProcessResult LaunchProcess(const fs::path& program,
	                            const std::vector<std::string>& args,
	                            const std::string& workingDirectory,
	                            bool captureOutput)
	{
		std::string commandLine = QuoteArgument(program.string());
		for (const auto& arg : args)
		{
			commandLine += ' ';
			commandLine += QuoteArgument(arg);
		}
		std::vector<char> mutableCommandLine(commandLine.begin(),
		                                     commandLine.end());
		mutableCommandLine.push_back('\0');

		HANDLE readPipe = nullptr;
		HANDLE writePipe = nullptr;
		STARTUPINFOA startupInfo{};
		startupInfo.cb = sizeof(startupInfo);

		if (captureOutput)
		{
			SECURITY_ATTRIBUTES sa{};
			sa.nLength = sizeof(sa);
			sa.bInheritHandle = TRUE;
			if (!CreatePipe(&readPipe, &writePipe, &sa, 0))
				throw std::runtime_error("CreatePipe failed for " +
				                         program.string());
			// The child inherits only the write end.
			SetHandleInformation(readPipe, HANDLE_FLAG_INHERIT, 0);
			startupInfo.dwFlags |= STARTF_USESTDHANDLES;
			startupInfo.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
			startupInfo.hStdOutput = writePipe;
			startupInfo.hStdError = writePipe;
		}

		PROCESS_INFORMATION processInfo{};
		BOOL created = CreateProcessA(
		    nullptr,
		    mutableCommandLine.data(),
		    nullptr,
		    nullptr,
		    captureOutput ? TRUE : FALSE,
		    0,
		    nullptr,
		    workingDirectory.empty() ? nullptr : workingDirectory.c_str(),
		    &startupInfo,
		    &processInfo);

		// The parent never writes to the pipe; closing its handle lets the read
		// end see EOF once the child exits.
		if (writePipe)
			CloseHandle(writePipe);
		if (!created)
		{
			if (readPipe)
				CloseHandle(readPipe);
			throw std::runtime_error("Cannot start process: " +
			                         program.string());
		}

		ProcessResult result;
		if (captureOutput)
		{
			char buffer[4096];
			DWORD read = 0;
			while (ReadFile(readPipe, buffer, sizeof(buffer), &read, nullptr) &&
			       read > 0)
				result.output.append(buffer, read);
			CloseHandle(readPipe);
		}

		WaitForSingleObject(processInfo.hProcess, INFINITE);
		DWORD exitCode = 0;
		GetExitCodeProcess(processInfo.hProcess, &exitCode);
		result.exitCode = static_cast<std::int32_t>(exitCode);
		CloseHandle(processInfo.hThread);
		CloseHandle(processInfo.hProcess);
		return result;
	}

	//-------------------------------------------------------------------------
	std::string RunProcess(const fs::path& program,
	                       const std::vector<std::string>& args)
	{
		auto result = LaunchProcess(program, args, {}, /*captureOutput*/ true);
		if (result.exitCode)
			throw std::runtime_error("Error when running: " + program.string() +
			                         " " + result.output);
		return result.output;
	}

	//------------------------------------------------------------------------ -
	fs::path GetVisualStudioPath()
	{
		fs::path programFileX86 = std::getenv("ProgramFiles(x86)");
		auto vswhere = programFileX86 / "Microsoft Visual Studio" /
		               "Installer" / "vswhere.exe";

		std::vector<std::string> args = {
		    "-latest", "-format", "value", "-property", "installationPath"};
		std::string value = RunProcess(vswhere, args);
		Tools::Trim(value);
		fs::path installerPath{value};

		if (!Tools::FileExists(installerPath))
			throw std::runtime_error(
			    "Invalid Visual Studio installation path: " + value);
		return installerPath;
	}
}