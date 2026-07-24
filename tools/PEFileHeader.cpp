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

#include "stdafx.h"
#include "PEFileHeader.hpp"

#include <format>

#include "ProcessMemory.hpp"

namespace Tools
{
	//-------------------------------------------------------------------------
	Expected<void> PEFileHeader::Load(HANDLE hProcess,
	                                  DWORD64 baseOfImage,
	                                  IPEFileHeaderHandler& handler) const
	{
		auto dosHeader = Tools::ReadStructInProcessMemory<IMAGE_DOS_HEADER>(
		    hProcess, baseOfImage);
		if (!dosHeader)
			return std::unexpected(std::move(dosHeader.error()));
		if ((*dosHeader)->e_magic != IMAGE_DOS_SIGNATURE)
			return std::unexpected(MAKE_ERROR("The image is not a valid DOS image."));
		auto ntHeader32 = Tools::ReadStructInProcessMemory<IMAGE_NT_HEADERS32>(
		    hProcess, baseOfImage + (*dosHeader)->e_lfanew);
		if (!ntHeader32)
			return std::unexpected(std::move(ntHeader32.error()));
		auto machine = (*ntHeader32)->FileHeader.Machine;

		if (machine == IMAGE_FILE_MACHINE_I386)
			handler.OnNtHeader32(hProcess, baseOfImage, **ntHeader32);
		else if (machine == IMAGE_FILE_MACHINE_AMD64)
		{
			auto ntHeader64 = ReadStructInProcessMemory<IMAGE_NT_HEADERS64>(
			    hProcess, baseOfImage + (*dosHeader)->e_lfanew);
			if (!ntHeader64)
				return std::unexpected(std::move(ntHeader64.error()));
			handler.OnNtHeader64(hProcess, baseOfImage, **ntHeader64);
		}
		else
			return std::unexpected(MAKE_ERROR(std::format(
			    L"PE file header machine is not supported: {}", machine)));
		return {};
	}
}
