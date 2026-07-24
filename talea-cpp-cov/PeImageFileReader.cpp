// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 natyamatsya and talea-cpp-cov contributors

#include "stdafx.h"
#include <cstdint>
#include "PeImageFileReader.hpp"

#include <Windows.h>

#include <mio/mmap.hpp>

namespace TaleaCppCov
{
	namespace
	{
		// One PE section, in RVA space with its backing file range.
		struct Section
		{
			std::uint32_t virtualAddress_;
			std::uint32_t virtualSize_;
			std::uint32_t rawOffset_;
			std::uint32_t rawSize_;
		};

		//---------------------------------------------------------------------
		template <typename T>
		const T* At(const mio::mmap_source& file, std::size_t offset)
		{
			if (offset > file.size() || file.size() - offset < sizeof(T))
				return nullptr;
			return reinterpret_cast<const T*>(file.data() + offset);
		}
	}

	//-------------------------------------------------------------------------
	class PeImageFileReader::Impl
	{
	  public:
		mio::mmap_source file_;
		std::vector<Section> sections_;
	};

	//-------------------------------------------------------------------------
	PeImageFileReader::PeImageFileReader(std::unique_ptr<Impl> impl)
	    : impl_{std::move(impl)}
	{
	}

	//-------------------------------------------------------------------------
	PeImageFileReader::~PeImageFileReader() = default;

	//-------------------------------------------------------------------------
	std::unique_ptr<PeImageFileReader>
	PeImageFileReader::TryCreate(const std::filesystem::path& path)
	{
		auto impl = std::make_unique<Impl>();

		std::error_code error;
		impl->file_ = mio::make_mmap_source(path.wstring(), error);
		if (error || impl->file_.empty())
			return nullptr;

		const auto& file = impl->file_;
		auto dosHeader = At<IMAGE_DOS_HEADER>(file, 0);
		if (!dosHeader || dosHeader->e_magic != IMAGE_DOS_SIGNATURE ||
		    dosHeader->e_lfanew < 0)
			return nullptr;

		auto ntOffset = static_cast<std::size_t>(dosHeader->e_lfanew);
		auto ntSignature = At<DWORD>(file, ntOffset);
		if (!ntSignature || *ntSignature != IMAGE_NT_SIGNATURE)
			return nullptr;

		auto fileHeader =
		    At<IMAGE_FILE_HEADER>(file, ntOffset + sizeof(DWORD));
		if (!fileHeader)
			return nullptr;

		auto sectionTableOffset = ntOffset + sizeof(DWORD) +
		                          sizeof(IMAGE_FILE_HEADER) +
		                          fileHeader->SizeOfOptionalHeader;

		impl->sections_.reserve(fileHeader->NumberOfSections);
		for (WORD i = 0; i < fileHeader->NumberOfSections; ++i)
		{
			auto section = At<IMAGE_SECTION_HEADER>(
			    file,
			    sectionTableOffset + i * sizeof(IMAGE_SECTION_HEADER));
			if (!section)
				return nullptr;

			impl->sections_.push_back(
			    Section{section->VirtualAddress,
			            section->Misc.VirtualSize,
			            section->PointerToRawData,
			            section->SizeOfRawData});
		}

		return std::unique_ptr<PeImageFileReader>(
		    new PeImageFileReader{std::move(impl)});
	}

	//-------------------------------------------------------------------------
	std::vector<std::uint8_t>
	PeImageFileReader::ReadCode(std::uint32_t rva, std::uint32_t size) const
	{
		if (size == 0)
			return {};

		for (const auto& section : impl_->sections_)
		{
			if (rva < section.virtualAddress_)
				continue;
			auto offsetInSection = rva - section.virtualAddress_;

			// The range must lie within both the virtual extent and the bytes
			// actually present in the file for this section.
			if (offsetInSection >= section.virtualSize_ ||
			    section.virtualSize_ - offsetInSection < size ||
			    offsetInSection >= section.rawSize_ ||
			    section.rawSize_ - offsetInSection < size)
			{
				continue;
			}

			auto fileOffset = static_cast<std::size_t>(section.rawOffset_) +
			                  offsetInSection;
			if (fileOffset > impl_->file_.size() ||
			    impl_->file_.size() - fileOffset < size)
			{
				return {};
			}

			auto* begin = reinterpret_cast<const std::uint8_t*>(
			    impl_->file_.data() + fileOffset);
			return std::vector<std::uint8_t>(begin, begin + size);
		}

		return {};
	}
}
