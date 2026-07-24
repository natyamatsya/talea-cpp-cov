// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2017 OpenCppCoverage

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
