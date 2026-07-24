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

#include "TestBasic.hpp"
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
#include <windows.h>

#include "tools/Tool.hpp"
#include "tools/ScopedAction.hpp"

#include "TestCoverageConsole.hpp"

namespace
{
	// Launches program with args and waits for it to exit. A tiny CreateProcess
	// wrapper so this target does not pull in a process-launching library.
	void LaunchAndWait(const std::filesystem::path& program,
	                   const std::vector<std::string>& args)
	{
		std::string commandLine = "\"" + program.string() + "\"";
		for (const auto& arg : args)
			commandLine += " \"" + arg + "\"";

		std::vector<char> buffer(commandLine.begin(), commandLine.end());
		buffer.push_back('\0');

		STARTUPINFOA startupInfo{};
		startupInfo.cb = sizeof(startupInfo);
		PROCESS_INFORMATION processInfo{};
		if (!CreateProcessA(nullptr, buffer.data(), nullptr, nullptr, FALSE, 0,
		                    nullptr, nullptr, &startupInfo, &processInfo))
			throw std::runtime_error("Cannot start child process.");

		WaitForSingleObject(processInfo.hProcess, INFINITE);
		CloseHandle(processInfo.hThread);
		CloseHandle(processInfo.hProcess);
	}
}

// We cannot move this code in the header, because the tests will not work in
// release mode. (This project has no optmisation in release mode).
namespace TestCoverageConsole
{
	//-------------------------------------------------------------------------
	void RunTestBasic() //! run-test-basic
	{
		if (false)
		{
			std::int32_t answer = 42;
		}
		volatile std::int32_t excludedLine = 42; // For CodeCoverageRunnerTest::ExcludedLine
	}

	//-------------------------------------------------------------------------
	void RunChildProcesses(int argc, _TCHAR* argv[])
	{
		auto outputBinaryPath = TestCoverageConsole::GetOutputBinaryPath();

		if (argc < 2)
			throw std::runtime_error("Invalid number of arguments.");
		if (argv[1] != TestCoverageConsole::TestChildProcess)
			throw std::runtime_error("Invalid argument.");

		for (int i = 2; i < argc; ++i)
		{
			auto argument = Tools::ToLocalString(argv[i]);
			std::cout << "Start: " << outputBinaryPath << " with " << argument << std::endl;
			LaunchAndWait(outputBinaryPath, std::vector<std::string>{argument});
		}
	}

	//-------------------------------------------------------------------------
	// Spawns a child that itself spawns a grandchild (ChildProcess TestBasic),
	// so the tree is two levels deep - exercises recursive child coverage.
	void RunNestedChildProcess()
	{
		auto outputBinaryPath = TestCoverageConsole::GetOutputBinaryPath();
		LaunchAndWait(
		    outputBinaryPath,
		    std::vector<std::string>{
		        Tools::ToLocalString(TestCoverageConsole::TestChildProcess),
		        Tools::ToLocalString(TestCoverageConsole::TestBasic)});
	}

	//-------------------------------------------------------------------------
	void UnloadReloadDll()
	{
		auto testHelper = L"TestHelper.dll";
		auto module = LoadLibraryEx(testHelper, nullptr, 0);
		
		// Be sure the library is unloaded by calling FreeLibrary until unload.
		while (FreeLibrary(module));

		module = LoadLibraryEx(testHelper, nullptr, 0);
		Tools::ScopedAction freeLibrary{ [=]() { FreeLibrary(module); } };

		auto fct = (void (*)())GetProcAddress(module, "TestUnloadDll");
		fct();	}
}
