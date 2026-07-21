// OpenCppCoverage is an open source code coverage for C++.
// Copyright (C) 2026 OpenCppCoverage
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
#include "ImportClosure.hpp"

#include <windows.h>

#include <cstdint>
#include <cwctype>
#include <set>
#include <string>

#include <mio/mmap.hpp>

namespace fs = std::filesystem;

namespace CppCoverage
{
	namespace
	{
		// Bound on how many modules the walk will visit, so a pathological
		// import graph cannot make startup unbounded.
		constexpr std::size_t MaxModules = 512;

		//---------------------------------------------------------------------
		std::wstring ToLower(std::wstring value)
		{
			for (auto& c : value)
				c = static_cast<wchar_t>(std::towlower(c));
			return value;
		}

		//---------------------------------------------------------------------
		// The names of the DLLs a PE image statically imports. Reads the on-disk
		// file directly (RVAs translated through the section table). Handles
		// both PE32 and PE32+ regardless of this build's bitness. Bounds-checked
		// against the mapped size; returns empty on anything malformed.
		std::vector<std::string> ReadImportedDllNames(const std::uint8_t* base,
		                                              std::size_t size)
		{
			std::vector<std::string> names;
			if (size < sizeof(IMAGE_DOS_HEADER))
				return names;

			auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
			if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew < 0)
				return names;
			auto ntOffset = static_cast<std::size_t>(dos->e_lfanew);
			if (ntOffset + sizeof(IMAGE_NT_HEADERS32) > size)
				return names;

			// FileHeader sits at the same offset in the 32- and 64-bit NT
			// headers, so this view is valid for section iteration either way.
			auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS32*>(base + ntOffset);
			if (nt->Signature != IMAGE_NT_SIGNATURE)
				return names;

			DWORD importRva = 0;
			auto magic = nt->OptionalHeader.Magic;
			if (magic == IMAGE_NT_OPTIONAL_HDR32_MAGIC)
			{
				importRva =
				    nt->OptionalHeader
				        .DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT]
				        .VirtualAddress;
			}
			else if (magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC)
			{
				if (ntOffset + sizeof(IMAGE_NT_HEADERS64) > size)
					return names;
				auto* nt64 = reinterpret_cast<const IMAGE_NT_HEADERS64*>(
				    base + ntOffset);
				importRva =
				    nt64->OptionalHeader
				        .DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT]
				        .VirtualAddress;
			}
			else
				return names;
			if (importRva == 0)
				return names;

			auto* firstSection = IMAGE_FIRST_SECTION(nt);
			auto rvaToPtr = [&](DWORD rva) -> const std::uint8_t* {
				auto* section = firstSection;
				for (WORD i = 0; i < nt->FileHeader.NumberOfSections;
				     ++i, ++section)
				{
					if (reinterpret_cast<const std::uint8_t*>(section + 1) >
					    base + size)
						return nullptr;
					auto va = section->VirtualAddress;
					auto rawSize = section->SizeOfRawData;
					if (rva >= va && rva < va + rawSize)
					{
						std::size_t offset =
						    static_cast<std::size_t>(section->PointerToRawData) +
						    (rva - va);
						if (offset < size)
							return base + offset;
					}
				}
				return nullptr;
			};

			auto readAsciiAt = [&](DWORD rva) -> std::string {
				std::string name;
				auto* p = rvaToPtr(rva);
				if (!p)
					return name;
				for (; p < base + size && *p; ++p)
					name.push_back(static_cast<char>(*p));
				return name;
			};

			auto* descPtr = rvaToPtr(importRva);
			if (!descPtr)
				return names;

			for (auto* desc =
			         reinterpret_cast<const IMAGE_IMPORT_DESCRIPTOR*>(descPtr);
			     reinterpret_cast<const std::uint8_t*>(desc + 1) <= base + size;
			     ++desc)
			{
				if (desc->Name == 0)
					break; // the all-zero terminating descriptor
				auto name = readAsciiAt(desc->Name);
				if (!name.empty())
					names.push_back(std::move(name));
			}
			return names;
		}
	}

	//-------------------------------------------------------------------------
	std::vector<fs::path>
	ComputeImportClosure(const fs::path& executablePath)
	{
		std::vector<fs::path> result;
		auto directory = executablePath.parent_path();

		std::set<std::wstring> visited;
		visited.insert(ToLower(executablePath.filename().wstring()));

		std::vector<fs::path> toScan{executablePath};
		while (!toScan.empty() && result.size() < MaxModules)
		{
			auto current = toScan.back();
			toScan.pop_back();

			std::error_code error;
			auto mapped = mio::make_mmap_source(current.wstring(), error);
			if (error || mapped.empty())
				continue;

			auto names = ReadImportedDllNames(
			    reinterpret_cast<const std::uint8_t*>(mapped.data()),
			    mapped.size());
			for (const auto& name : names)
			{
				// DLL import names are ASCII.
				std::wstring wideName(name.begin(), name.end());
				auto key = ToLower(wideName);
				if (!visited.insert(key).second)
					continue;

				// Only the app's own DLLs (next to the exe) are relevant; system
				// imports resolve elsewhere and are skipped.
				auto candidate = directory / wideName;
				if (fs::exists(candidate, error))
				{
					result.push_back(candidate);
					toScan.push_back(candidate); // walk its imports too
				}
			}
		}
		return result;
	}
}
