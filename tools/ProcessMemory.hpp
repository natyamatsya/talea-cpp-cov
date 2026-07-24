// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2017 OpenCppCoverage

#pragma once
#include <cstdint>

#include <Windows.h>
#include <memory>
#include <vector>

#include "ToolsExport.hpp"
#include "Error.hpp"

namespace Tools
{
	[[nodiscard]] TOOLS_DLL Expected<void> WriteProcessMemory(HANDLE hProcess,
	                                                          void* address,
	                                                          void* buffer,
	                                                          size_t size);

	[[nodiscard]] TOOLS_DLL Expected<std::vector<std::uint8_t>>
	ReadProcessMemory(HANDLE hProcess, void* address, size_t size);

	[[nodiscard]] TOOLS_DLL Expected<void> ReadProcessMemory(HANDLE hProcess,
	                                                         DWORD64 address,
	                                                         void* buffer,
	                                                         SIZE_T size);

	//-------------------------------------------------------------------------
	template <typename T>
	[[nodiscard]] Expected<std::unique_ptr<T>>
	ReadStructInProcessMemory(HANDLE hProcess, DWORD64 address)
	{
		auto data = std::make_unique<T>();
		auto result =
		    ::Tools::ReadProcessMemory(hProcess, address, data.get(), sizeof(T));
		if (!result)
			return std::unexpected(std::move(result.error()));

		return data;
	}
}