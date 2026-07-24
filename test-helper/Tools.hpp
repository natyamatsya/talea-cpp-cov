// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2016 OpenCppCoverage

#pragma once
#include <cstdint>

#include <filesystem>
#include "TestHelperExport.hpp"

namespace TestHelper
{
	std::filesystem::path TEST_HELPER_DLL  GetTestUnloadDllFilename();
	std::filesystem::path TEST_HELPER_DLL  GetOutputBinaryPath();

	void TEST_HELPER_DLL CreateEmptyFile(const std::filesystem::path&);

	struct ProcessResult
	{
		std::int32_t exitCode = 0;
		std::string output; // stdout + stderr, only when captureOutput is set
	};

	// Launches program with args (each argument quoted as needed) and waits for
	// it to exit. workingDirectory may be empty (inherit the caller's). When
	// captureOutput is set, the child's stdout and stderr are captured.
	ProcessResult TEST_HELPER_DLL LaunchProcess(
		const std::filesystem::path& program,
		const std::vector<std::string>& args,
		const std::string& workingDirectory,
		bool captureOutput);

	std::string TEST_HELPER_DLL RunProcess(
		const std::filesystem::path& program,
		const std::vector<std::string>& args);

	std::filesystem::path TEST_HELPER_DLL GetVisualStudioPath();

	//-------------------------------------------------------------------------
	template <typename ExceptionType, typename Fct>
	void AssertThrow(Fct fct,
	                 std::function<void(const ExceptionType&)> exceptionCheck)
	{
		try
		{
			fct();
		}
		catch (const ExceptionType& e)
		{
			exceptionCheck(e);
			return;
		}
		catch (...)
		{
			throw std::runtime_error("Expected exception not raised.");
		}
		throw std::runtime_error("No exception raised.");
	}
}
