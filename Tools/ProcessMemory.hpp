// OpenCppCoverage is an open source code coverage for C++.
// Copyright (C) 2017 OpenCppCoverage
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

#pragma once
#include <cstdint>

#include <Windows.h>
#include <memory>
#include <vector>

#include "ToolsExport.hpp"
#include "Error.hpp"

namespace Tools
{
	TOOLS_DLL Expected<void> WriteProcessMemory(HANDLE hProcess,
	                                            void* address,
	                                            void* buffer,
	                                            size_t size);

	TOOLS_DLL Expected<std::vector<std::uint8_t>>
	ReadProcessMemory(HANDLE hProcess, void* address, size_t size);

	TOOLS_DLL Expected<void> ReadProcessMemory(HANDLE hProcess,
	                                           DWORD64 address,
	                                           void* buffer,
	                                           SIZE_T size);

	//-------------------------------------------------------------------------
	template <typename T>
	Expected<std::unique_ptr<T>> ReadStructInProcessMemory(HANDLE hProcess,
	                                                       DWORD64 address)
	{
		auto data = std::make_unique<T>();
		auto result =
		    ::Tools::ReadProcessMemory(hProcess, address, data.get(), sizeof(T));
		if (!result)
			return std::unexpected(std::move(result.error()));

		return data;
	}
}