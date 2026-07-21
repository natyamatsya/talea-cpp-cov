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
#include "RawPdbReader.hpp"

#include <algorithm>
#include <cstring>
#include <execution>
#include <unordered_map>
#include <vector>

#include <Windows.h>
#include <mio/mmap.hpp>

#include "PdbSignature.hpp"
#include "PdbLineTableCache.hpp"

#include "PDB.h"
#include "PDB_RawFile.h"
#include "PDB_InfoStream.h"
#include "PDB_DBIStream.h"
#include "PDB_ImageSectionStream.h"
#include "PDB_ModuleInfoStream.h"
#include "PDB_ModuleLineStream.h"
#include "PDB_NamesStream.h"

namespace CppCoverage
{
	namespace
	{
		// DIA drops these compiler-generated markers; match it.
		constexpr unsigned long InvalidLine1 = 0x00f00f00;
		constexpr unsigned long InvalidLine2 = 0x00feefee;

		//---------------------------------------------------------------------
		std::wstring Utf8ToWide(const char* utf8)
		{
			if (!utf8 || !*utf8)
				return {};
			int length = MultiByteToWideChar(
			    CP_UTF8, 0, utf8, -1, nullptr, 0);
			if (length <= 1)
				return {};
			std::wstring result(static_cast<std::size_t>(length - 1), L'\0');
			MultiByteToWideChar(
			    CP_UTF8, 0, utf8, -1, result.data(), length);
			return result;
		}

		//---------------------------------------------------------------------
		// Tries the PDB path recorded in the binary, then the same file name
		// next to the binary, then <binary stem>.pdb next to it.
		std::optional<std::filesystem::path>
		ResolvePdbPath(const std::filesystem::path& binaryPath,
		               const std::wstring& recordedPath)
		{
			std::error_code error;
			if (!recordedPath.empty())
			{
				std::filesystem::path recorded{recordedPath};
				if (std::filesystem::exists(recorded, error))
					return recorded;

				auto sideBySide =
				    binaryPath.parent_path() / recorded.filename();
				if (std::filesystem::exists(sideBySide, error))
					return sideBySide;
			}

			auto renamed = binaryPath;
			renamed.replace_extension(L".pdb");
			if (std::filesystem::exists(renamed, error))
				return renamed;

			return std::nullopt;
		}

		//---------------------------------------------------------------------
		bool SignatureMatches(const PDB::InfoStream& infoStream,
		                      const PdbSignature& expected)
		{
			const auto* header = infoStream.GetHeader();
			if (!header)
				return false;
			return header->age == expected.age_ &&
			       std::memcmp(&header->guid, &expected.guid_,
			                   sizeof(GUID)) == 0;
		}

		//---------------------------------------------------------------------
		// One compiland's contribution: line records grouped by source file
		// (raw PDB name), before merging into the module table.
		struct ModuleContribution
		{
			std::vector<ModuleLineTable::File> files_;
			std::vector<IDebugInformationHandler::FunctionRange> functions_;
		};

		//---------------------------------------------------------------------
		ModuleContribution ReadModule(
		    const PDB::RawFile& rawFile,
		    const PDB::ImageSectionStream& sectionStream,
		    const PDB::NamesStream& namesStream,
		    const PDB::ModuleInfoStream::Module& module)
		{
			ModuleContribution contribution;

			// --- line records, grouped by source-file name ------------------
			if (module.HasLineStream())
			{
				const auto lineStream = module.CreateLineStream(rawFile);

				// A line block references its file by a checksum offset; the
				// checksum -> filename map lives in a separate subsection that
				// may appear after the line blocks, so resolve names in a
				// second pass.
				struct PendingBlock
				{
					std::uint32_t fileChecksumOffset_;
					std::vector<IDebugInformationHandler::Line> lines_;
				};
				std::vector<PendingBlock> pendingBlocks;
				const PDB::CodeView::DBI::FileChecksumHeader* checksumHeader =
				    nullptr;

				lineStream.ForEachSection(
				    [&](const PDB::CodeView::DBI::LineSection* lineSection) {
					    if (lineSection->header.kind ==
					        PDB::CodeView::DBI::DebugSubsectionKind::S_LINES)
					    {
						    const std::uint16_t sectionIndex =
						        lineSection->linesHeader.sectionIndex;
						    const std::uint32_t sectionOffset =
						        lineSection->linesHeader.sectionOffset;
						    const std::uint32_t blockCodeSize =
						        lineSection->linesHeader.codeSize;

						    lineStream.ForEachLinesBlock(
						        lineSection,
						        [&](const PDB::CodeView::DBI::
						                LinesFileBlockHeader* blockHeader,
						            const PDB::CodeView::DBI::Line* blockLines,
						            const PDB::CodeView::DBI::Column*) {
							        if (blockHeader->numLines == 0)
								        return;

							        PendingBlock pending;
							        pending.fileChecksumOffset_ =
							            blockHeader->fileChecksumOffset;
							        pending.lines_.reserve(
							            blockHeader->numLines);
							        for (std::uint32_t i = 0;
							             i < blockHeader->numLines;
							             ++i)
							        {
								        const auto& line = blockLines[i];
								        auto lineNumber =
								            line.linenumStart;
								        if (lineNumber == InvalidLine1 ||
								            lineNumber == InvalidLine2)
									        continue;

								        auto rva =
								            sectionStream
								                .ConvertSectionOffsetToRVA(
								                    sectionIndex,
								                    sectionOffset +
								                        line.offset);
								        // Code length: to the next line, or
								        // to the end of the block for the last.
								        std::uint32_t next =
								            (i + 1 < blockHeader->numLines)
								                ? blockLines[i + 1].offset
								                : blockCodeSize;
								        std::uint32_t length =
								            next - line.offset;
								        pending.lines_.emplace_back(
								            lineNumber,
								            static_cast<std::int64_t>(rva),
								            0ul,
								            length);
							        }
							        if (!pending.lines_.empty())
								        pendingBlocks.push_back(
								            std::move(pending));
						        });
					    }
					    else if (lineSection->header.kind ==
					             PDB::CodeView::DBI::DebugSubsectionKind::
					                 S_FILECHECKSUMS)
					    {
						    checksumHeader = &lineSection->checksumHeader;
					    }
				    });

				// Resolve each block's file name and merge by name.
				std::unordered_map<std::wstring, std::size_t> fileIndexByName;
				for (auto& block : pendingBlocks)
				{
					std::wstring name;
					if (checksumHeader)
					{
						const auto* fileChecksum =
						    reinterpret_cast<
						        const PDB::CodeView::DBI::FileChecksumHeader*>(
						        reinterpret_cast<const std::uint8_t*>(
						            checksumHeader) +
						        block.fileChecksumOffset_);
						name = Utf8ToWide(namesStream.GetFilename(
						    fileChecksum->filenameOffset));
					}

					auto [it, inserted] = fileIndexByName.emplace(
					    name, contribution.files_.size());
					if (inserted)
						contribution.files_.push_back(
						    ModuleLineTable::File{name, {}});
					auto& target = contribution.files_[it->second].lines_;
					target.insert(target.end(),
					              block.lines_.begin(),
					              block.lines_.end());
				}
			}

			// --- function ranges -------------------------------------------
			const auto symbolStream = module.CreateSymbolStream(rawFile);
			symbolStream.ForEachSymbol(
			    [&](const PDB::CodeView::DBI::Record* record) {
				    std::uint16_t section = 0;
				    std::uint32_t offset = 0;
				    std::uint32_t codeSize = 0;
				    switch (record->header.kind)
				    {
					    case PDB::CodeView::DBI::SymbolRecordKind::S_GPROC32:
					    case PDB::CodeView::DBI::SymbolRecordKind::S_LPROC32:
					    case PDB::CodeView::DBI::SymbolRecordKind::
					        S_GPROC32_ID:
					    case PDB::CodeView::DBI::SymbolRecordKind::
					        S_LPROC32_ID:
						    section = record->data.S_GPROC32.section;
						    offset = record->data.S_GPROC32.offset;
						    codeSize = record->data.S_GPROC32.codeSize;
						    break;
					    default:
						    return;
				    }
				    if (codeSize == 0)
					    return;
				    auto rva = sectionStream.ConvertSectionOffsetToRVA(
				        section, offset);
				    contribution.functions_.push_back(
				        {static_cast<std::int64_t>(rva), codeSize, {}});
			    });

			return contribution;
		}
	}

	//-------------------------------------------------------------------------
	bool RawPdbReader::TryRead(const std::filesystem::path& binaryPath,
	                           ModuleLineTable& out)
	{
		auto signature = ExtractPdbSignature(binaryPath);
		if (!signature)
			return false;

		auto pdbPath = ResolvePdbPath(binaryPath, signature->pdbPath_);
		if (!pdbPath)
			return false;

		std::error_code error;
		auto mappedFile = mio::make_mmap_source(pdbPath->wstring(), error);
		if (error || mappedFile.empty())
			return false;

		if (PDB::ValidateFile(mappedFile.data(), mappedFile.size()) !=
		    PDB::ErrorCode::Success)
			return false;

		const PDB::RawFile rawFile = PDB::CreateRawFile(mappedFile.data());
		if (PDB::HasValidDBIStream(rawFile) != PDB::ErrorCode::Success)
			return false;

		const PDB::InfoStream infoStream(rawFile);
		// FASTLINK line tables need the linker at read time; leave those to DIA.
		if (infoStream.UsesDebugFastLink() ||
		    !infoStream.HasNamesStream() ||
		    !SignatureMatches(infoStream, *signature))
			return false;

		const PDB::DBIStream dbiStream = PDB::CreateDBIStream(rawFile);
		if (dbiStream.HasValidImageSectionStream(rawFile) !=
		        PDB::ErrorCode::Success ||
		    dbiStream.HasValidSymbolRecordStream(rawFile) !=
		        PDB::ErrorCode::Success)
			return false;

		const auto sectionStream =
		    dbiStream.CreateImageSectionStream(rawFile);
		const auto moduleInfoStream =
		    dbiStream.CreateModuleInfoStream(rawFile);
		const auto namesStream = infoStream.CreateNamesStream(rawFile);

		const auto modules = moduleInfoStream.GetModules();

		// Each compiland's streams are independent immutable memory, so parse
		// them in parallel (this is why raw_pdb is used, not COM-bound DIA).
		std::vector<ModuleContribution> contributions(modules.GetLength());
		std::vector<std::size_t> indices(modules.GetLength());
		for (std::size_t i = 0; i < indices.size(); ++i)
			indices[i] = i;

		std::for_each(
		    std::execution::par,
		    indices.begin(),
		    indices.end(),
		    [&](std::size_t i) {
			    contributions[i] =
			        ReadModule(rawFile, sectionStream, namesStream,
			                   modules[i]);
		    });

		// Merge the per-compiland contributions into the module table,
		// coalescing files by name across compilands.
		out.files_.clear();
		out.functions_.clear();
		out.hasSymbolIndexes_ = false;
		std::unordered_map<std::wstring, std::size_t> fileIndexByName;
		for (auto& contribution : contributions)
		{
			for (auto& file : contribution.files_)
			{
				auto [it, inserted] = fileIndexByName.emplace(
				    file.rawName_, out.files_.size());
				if (inserted)
					out.files_.push_back(
					    ModuleLineTable::File{file.rawName_, {}});
				auto& target = out.files_[it->second].lines_;
				target.insert(target.end(),
				              file.lines_.begin(),
				              file.lines_.end());
			}
			out.functions_.insert(out.functions_.end(),
			                      contribution.functions_.begin(),
			                      contribution.functions_.end());
		}

		std::sort(out.functions_.begin(),
		          out.functions_.end(),
		          [](const auto& lhs, const auto& rhs) {
			          return lhs.virtualAddress_ < rhs.virtualAddress_;
		          });

		return true;
	}
}
