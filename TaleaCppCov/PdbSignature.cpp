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
#include <cstdint>
#include "PdbSignature.hpp"

#include <mio/mmap.hpp>

namespace TaleaCppCov
{
	namespace
	{
		//---------------------------------------------------------------------
		// Bounds-checked view over the mapped binary: every structure access
		// is validated against the file size, so a truncated or corrupt PE
		// yields nullopt instead of an out-of-bounds read.
		struct FileView
		{
			const char* data_;
			size_t size_;

			template <typename T>
			const T* At(size_t offset) const
			{
				if (offset > size_ || size_ - offset < sizeof(T))
					return nullptr;
				return reinterpret_cast<const T*>(data_ + offset);
			}
		};

		//---------------------------------------------------------------------
		// CV_INFO_PDB70: the CodeView debug entry written by modern linkers.
		// The PDB path follows the age but is irrelevant for identity.
		struct CodeViewRecord
		{
			DWORD signature_;
			GUID guid_;
			DWORD age_;
		};
		constexpr DWORD RsdsSignature = 0x53445352; // "RSDS"

		//---------------------------------------------------------------------
		std::optional<IMAGE_DATA_DIRECTORY>
		GetDebugDirectory(const FileView& view,
		                  size_t optionalHeaderOffset)
		{
			auto magic = view.At<WORD>(optionalHeaderOffset);
			if (!magic)
				return std::nullopt;

			if (*magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC)
			{
				auto header =
				    view.At<IMAGE_OPTIONAL_HEADER64>(optionalHeaderOffset);
				if (header &&
				    header->NumberOfRvaAndSizes > IMAGE_DIRECTORY_ENTRY_DEBUG)
					return header->DataDirectory[IMAGE_DIRECTORY_ENTRY_DEBUG];
			}
			else if (*magic == IMAGE_NT_OPTIONAL_HDR32_MAGIC)
			{
				auto header =
				    view.At<IMAGE_OPTIONAL_HEADER32>(optionalHeaderOffset);
				if (header &&
				    header->NumberOfRvaAndSizes > IMAGE_DIRECTORY_ENTRY_DEBUG)
					return header->DataDirectory[IMAGE_DIRECTORY_ENTRY_DEBUG];
			}

			return std::nullopt;
		}

		//---------------------------------------------------------------------
		std::optional<size_t> RvaToFileOffset(const FileView& view,
		                                      size_t sectionTableOffset,
		                                      WORD sectionCount,
		                                      DWORD rva)
		{
			for (WORD i = 0; i < sectionCount; ++i)
			{
				auto section = view.At<IMAGE_SECTION_HEADER>(
				    sectionTableOffset + i * sizeof(IMAGE_SECTION_HEADER));
				if (!section)
					return std::nullopt;

				if (rva >= section->VirtualAddress &&
				    rva - section->VirtualAddress < section->SizeOfRawData)
				{
					return rva - section->VirtualAddress +
					       section->PointerToRawData;
				}
			}

			return std::nullopt;
		}

		//---------------------------------------------------------------------
		std::optional<PdbSignature> ExtractFromView(const FileView& view)
		{
			auto dosHeader = view.At<IMAGE_DOS_HEADER>(0);
			if (!dosHeader || dosHeader->e_magic != IMAGE_DOS_SIGNATURE ||
			    dosHeader->e_lfanew < 0)
				return std::nullopt;

			auto ntOffset = static_cast<size_t>(dosHeader->e_lfanew);
			auto ntSignature = view.At<DWORD>(ntOffset);
			if (!ntSignature || *ntSignature != IMAGE_NT_SIGNATURE)
				return std::nullopt;

			auto fileHeader =
			    view.At<IMAGE_FILE_HEADER>(ntOffset + sizeof(DWORD));
			if (!fileHeader)
				return std::nullopt;

			auto optionalHeaderOffset =
			    ntOffset + sizeof(DWORD) + sizeof(IMAGE_FILE_HEADER);
			auto debugDirectory =
			    GetDebugDirectory(view, optionalHeaderOffset);
			if (!debugDirectory || !debugDirectory->VirtualAddress ||
			    !debugDirectory->Size)
				return std::nullopt;

			auto debugOffset = RvaToFileOffset(
			    view,
			    optionalHeaderOffset + fileHeader->SizeOfOptionalHeader,
			    fileHeader->NumberOfSections,
			    debugDirectory->VirtualAddress);
			if (!debugOffset)
				return std::nullopt;

			auto entryCount =
			    debugDirectory->Size / sizeof(IMAGE_DEBUG_DIRECTORY);
			for (size_t i = 0; i < entryCount; ++i)
			{
				auto entry = view.At<IMAGE_DEBUG_DIRECTORY>(
				    *debugOffset + i * sizeof(IMAGE_DEBUG_DIRECTORY));
				if (!entry)
					return std::nullopt;

				if (entry->Type != IMAGE_DEBUG_TYPE_CODEVIEW ||
				    entry->SizeOfData < sizeof(CodeViewRecord))
					continue;

				// PointerToRawData is already a file offset.
				auto record =
				    view.At<CodeViewRecord>(entry->PointerToRawData);
				if (record && record->signature_ == RsdsSignature)
				{
					PdbSignature signature{record->guid_, record->age_, {}};

					// The UTF-8, null-terminated PDB path follows the record,
					// bounded by the entry's data size.
					auto pathOffset =
					    entry->PointerToRawData + sizeof(CodeViewRecord);
					auto pathMax = entry->SizeOfData - sizeof(CodeViewRecord);
					auto* pathBytes = view.At<char>(pathOffset);
					if (pathBytes)
					{
						size_t length = 0;
						while (length < pathMax &&
						       pathOffset + length < view.size_ &&
						       pathBytes[length] != '\0')
							++length;
						std::string utf8{pathBytes, pathBytes + length};
						std::int32_t wideLength = MultiByteToWideChar(
						    CP_UTF8, 0, utf8.data(),
						    static_cast<std::int32_t>(utf8.size()), nullptr, 0);
						if (wideLength > 0)
						{
							signature.pdbPath_.resize(wideLength);
							MultiByteToWideChar(
							    CP_UTF8, 0, utf8.data(),
							    static_cast<std::int32_t>(utf8.size()),
							    signature.pdbPath_.data(), wideLength);
						}
					}
					return signature;
				}
			}

			return std::nullopt;
		}
	}

	//-------------------------------------------------------------------------
	std::wstring PdbSignature::ToString() const
	{
		wchar_t buffer[64];
		swprintf_s(buffer,
		           L"%08lx%04x%04x%02x%02x%02x%02x%02x%02x%02x%02x-%lu",
		           guid_.Data1,
		           guid_.Data2,
		           guid_.Data3,
		           guid_.Data4[0],
		           guid_.Data4[1],
		           guid_.Data4[2],
		           guid_.Data4[3],
		           guid_.Data4[4],
		           guid_.Data4[5],
		           guid_.Data4[6],
		           guid_.Data4[7],
		           age_);
		return buffer;
	}

	//-------------------------------------------------------------------------
	std::optional<PdbSignature>
	ExtractPdbSignature(const std::filesystem::path& binaryPath)
	{
		// Loaded images lock writes, not reads, so mapping the module file
		// of a running debuggee is safe.
		std::error_code error;
		auto mappedFile = mio::make_mmap_source(binaryPath.wstring(), error);
		if (error || mappedFile.empty())
			return std::nullopt;

		return ExtractFromView({mappedFile.data(), mappedFile.size()});
	}
}
