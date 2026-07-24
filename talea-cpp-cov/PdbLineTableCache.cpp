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
#include "PdbLineTableCache.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <format>
#include <fstream>
#include <type_traits>

#include <mio/mmap.hpp>

namespace TaleaCppCov
{
	namespace
	{
		// Cache file layout, version 3 (little-endian). Metadata first, then
		// one 8-aligned blob of raw Line records, so the blob can be consumed
		// IN PLACE from a file mapping (see LineTableView):
		//   u32 magic, u32 version, u32 flags (bit 0: hasSymbolIndexes),
		//   u32 fileCount, u64 totalLineCount, u32 functionCount, u32 pad,
		//   then per function: i64 virtualAddress, u32 length, u32 pad,
		//   then per file: u32 nameChars, u32 lineCount, u64 firstLine,
		//                  wchar_t[nameChars],
		//   then zero-padding up to the next 8-byte boundary,
		//   then Line[totalLineCount] - the in-memory representation, copied
		//   verbatim (the static_asserts below pin that contract).
		constexpr uint32_t CacheMagic = 0x4350434F; // "OCPC"
		constexpr uint32_t CacheVersion = 3;
		constexpr auto CacheFileExtension = L".lines";
		constexpr auto TempFileExtension = L".tmp";
		using Line = IDebugInformationHandler::Line;
		static_assert(std::is_trivially_copyable_v<Line>,
		              "Line is memcpy'd to and reinterpreted from the cache "
		              "file blob.");
		static_assert(sizeof(Line) == 24 && alignof(Line) == 8,
		              "The v3 cache layout fixes Line's size and alignment; "
		              "bump CacheVersion when the struct changes.");
		constexpr size_t SerializedFunctionSize =
		    sizeof(int64_t) + 2 * sizeof(uint32_t);
		constexpr size_t SerializedFileHeaderSize =
		    2 * sizeof(uint32_t) + sizeof(uint64_t);

		// The identity key makes stale entries harmless but they still take
		// disk space: cap the directory and drop the oldest entries.
		constexpr size_t MaxDiskCacheEntries = 256;

		//---------------------------------------------------------------------
		void Append(std::vector<char>& buffer, const void* data, size_t size)
		{
			auto begin = static_cast<const char*>(data);
			buffer.insert(buffer.end(), begin, begin + size);
		}

		template <typename T>
		void AppendValue(std::vector<char>& buffer, T value)
		{
			Append(buffer, &value, sizeof(value));
		}

		//---------------------------------------------------------------------
		// Sequential bounds-checked reader: any overrun latches failed_ and
		// makes every later read a no-op, so one final check suffices.
		struct Reader
		{
			const char* position_;
			const char* end_;
			bool failed_ = false;

			bool ReadInto(void* destination, size_t size)
			{
				if (failed_ ||
				    static_cast<size_t>(end_ - position_) < size)
				{
					failed_ = true;
					return false;
				}
				std::memcpy(destination, position_, size);
				position_ += size;
				return true;
			}

			template <typename T>
			T Read()
			{
				T value{};
				ReadInto(&value, sizeof(T));
				return value;
			}

			size_t Remaining() const
			{
				return failed_ ? 0
				               : static_cast<size_t>(end_ - position_);
			}
		};
	}

	//-------------------------------------------------------------------------
	LineTableView::LineTableView(std::shared_ptr<const ModuleLineTable> table)
	    : table_{std::move(table)}
	{
	}

	//-------------------------------------------------------------------------
	bool LineTableView::HasSymbolIndexes() const
	{
		return table_ ? table_->hasSymbolIndexes_ : mappedHasSymbolIndexes_;
	}

	//-------------------------------------------------------------------------
	const std::vector<LineTableView::FunctionRange>&
	LineTableView::Functions() const
	{
		return table_ ? table_->functions_ : mappedFunctions_;
	}

	//-------------------------------------------------------------------------
	std::size_t LineTableView::FileCount() const
	{
		return table_ ? table_->files_.size() : mappedFiles_.size();
	}

	//-------------------------------------------------------------------------
	const std::wstring&
	LineTableView::FileRawName(std::size_t fileIndex) const
	{
		return table_ ? table_->files_[fileIndex].rawName_
		              : mappedFiles_[fileIndex].rawName_;
	}

	//-------------------------------------------------------------------------
	const std::vector<LineTableView::Line>&
	LineTableView::FileLines(std::size_t fileIndex,
	                         std::vector<Line>& scratch) const
	{
		if (table_)
			return table_->files_[fileIndex].lines_;

		const auto& file = mappedFiles_[fileIndex];
		scratch.assign(mappedLines_ + file.firstLine_,
		               mappedLines_ + file.firstLine_ + file.lineCount_);
		return scratch;
	}

	//-------------------------------------------------------------------------
	PdbLineTableCache::PdbLineTableCache(
	    std::filesystem::path diskCacheDirectory)
	    : diskCacheDirectory_{std::move(diskCacheDirectory)}
	{
	}

	//-------------------------------------------------------------------------
	std::filesystem::path PdbLineTableCache::GetDefaultDiskCacheDirectory()
	{
		wchar_t buffer[MAX_PATH];
		auto length =
		    GetEnvironmentVariableW(L"LOCALAPPDATA", buffer, MAX_PATH);
		std::filesystem::path base;
		if (length != 0 && length < MAX_PATH)
			base = buffer;
		else
			base = std::filesystem::temp_directory_path();
		return base / L"TaleaCppCovApp" / L"PdbCache";
	}

	//-------------------------------------------------------------------------
	std::shared_ptr<const LineTableView>
	PdbLineTableCache::TryGet(const PdbSignature& signature,
	                          bool needSymbolIndexes)
	{
		std::lock_guard<std::mutex> lock(mutex_);
		auto key = signature.ToString();
		auto it = views_.find(key);
		if (it != views_.end() &&
		    (it->second->HasSymbolIndexes() || !needSymbolIndexes))
		{
			++memoryHitCount_;
			return it->second;
		}

		if (!diskCacheDirectory_.empty())
		{
			auto view = TryLoadFromDisk(GetCacheFilePath(signature, true));
			if (!view && !needSymbolIndexes)
				view = TryLoadFromDisk(GetCacheFilePath(signature, false));
			if (view && (view->HasSymbolIndexes() || !needSymbolIndexes))
			{
				++diskHitCount_;
				views_[key] = view;
				return view;
			}
		}

		++missCount_;
		return nullptr;
	}

	//-------------------------------------------------------------------------
	void
	PdbLineTableCache::Store(const PdbSignature& signature,
	                         std::shared_ptr<const ModuleLineTable> table)
	{
		if (!table)
			return;

		std::lock_guard<std::mutex> lock(mutex_);
		auto key = signature.ToString();
		auto it = views_.find(key);

		// Never replace an entry carrying symbol indexes with one without.
		if (it != views_.end() && it->second->HasSymbolIndexes() &&
		    !table->hasSymbolIndexes_)
			return;

		// Prefer a mapping of the just-written cache file over the heap
		// table: the heavy line blob then lives in file-backed pages from
		// the first run on, not just on replays (MEMORY_ROADMAP.md, Tier 1).
		std::shared_ptr<const LineTableView> view;
		if (!diskCacheDirectory_.empty() && SaveToDisk(signature, *table))
			view = TryLoadFromDisk(
			    GetCacheFilePath(signature, table->hasSymbolIndexes_));
		if (!view)
			view = std::make_shared<LineTableView>(std::move(table));
		views_[key] = std::move(view);
	}

	//-------------------------------------------------------------------------
	std::int32_t PdbLineTableCache::GetMemoryHitCount() const
	{
		std::lock_guard<std::mutex> lock(mutex_);
		return memoryHitCount_;
	}

	//-------------------------------------------------------------------------
	std::int32_t PdbLineTableCache::GetDiskHitCount() const
	{
		std::lock_guard<std::mutex> lock(mutex_);
		return diskHitCount_;
	}

	//-------------------------------------------------------------------------
	std::int32_t PdbLineTableCache::GetMissCount() const
	{
		std::lock_guard<std::mutex> lock(mutex_);
		return missCount_;
	}

	//-------------------------------------------------------------------------
	std::filesystem::path
	PdbLineTableCache::GetCacheFilePath(const PdbSignature& signature,
	                                    bool withSymbolIndexes) const
	{
		// Distinct names let the plain and symbol-index variants coexist.
		auto filename = signature.ToString();
		if (withSymbolIndexes)
			filename += L"-sym";
		filename += CacheFileExtension;
		return diskCacheDirectory_ / filename;
	}

	//-------------------------------------------------------------------------
	std::shared_ptr<const LineTableView> PdbLineTableCache::TryLoadFromDisk(
	    const std::filesystem::path& path) const
	{
		std::error_code error;
		auto mappedFile = std::make_shared<mio::mmap_source>(
		    mio::make_mmap_source(path.wstring(), error));
		if (error || mappedFile->empty())
			return nullptr;

		const char* base = mappedFile->data();
		Reader reader{base, base + mappedFile->size()};
		if (reader.Read<uint32_t>() != CacheMagic ||
		    reader.Read<uint32_t>() != CacheVersion)
			return nullptr;

		auto view = std::shared_ptr<LineTableView>(new LineTableView());
		view->mappedHasSymbolIndexes_ =
		    (reader.Read<uint32_t>() & 1u) != 0;

		// Counts are validated against the remaining bytes before any
		// reserve, so a corrupt file cannot request absurd allocations.
		auto fileCount = reader.Read<uint32_t>();
		auto totalLineCount = reader.Read<uint64_t>();
		auto functionCount = reader.Read<uint32_t>();
		reader.Read<uint32_t>(); // pad
		if (reader.failed_ ||
		    functionCount > reader.Remaining() / SerializedFunctionSize)
			return nullptr;

		view->mappedFunctions_.reserve(functionCount);
		for (uint32_t functionIndex = 0; functionIndex < functionCount;
		     ++functionIndex)
		{
			auto virtualAddress = reader.Read<int64_t>();
			auto length = reader.Read<uint32_t>();
			reader.Read<uint32_t>(); // pad
			view->mappedFunctions_.push_back(
			    {virtualAddress, static_cast<std::uint32_t>(length)});
		}

		if (fileCount > reader.Remaining() / SerializedFileHeaderSize)
			return nullptr;
		view->mappedFiles_.reserve(fileCount);
		uint64_t expectedLines = 0;
		for (uint32_t fileIndex = 0; fileIndex < fileCount; ++fileIndex)
		{
			LineTableView::MappedFile file;
			auto nameChars = reader.Read<uint32_t>();
			file.lineCount_ = reader.Read<uint32_t>();
			file.firstLine_ = reader.Read<uint64_t>();
			if (reader.failed_ ||
			    nameChars > reader.Remaining() / sizeof(wchar_t))
				return nullptr;
			file.rawName_.resize(nameChars);
			reader.ReadInto(file.rawName_.data(),
			                nameChars * sizeof(wchar_t));
			if (file.firstLine_ > totalLineCount ||
			    file.lineCount_ > totalLineCount - file.firstLine_)
				return nullptr;
			expectedLines += file.lineCount_;
			view->mappedFiles_.push_back(std::move(file));
		}

		// The line blob starts at the next 8-byte boundary and must fill the
		// file exactly. It is consumed in place: mappedLines_ points into the
		// mapping, which the view keeps alive.
		auto metadataSize =
		    static_cast<size_t>(reader.position_ - base);
		auto blobOffset = (metadataSize + 7) & ~size_t{7};
		if (reader.failed_ || expectedLines != totalLineCount ||
		    blobOffset > mappedFile->size() ||
		    mappedFile->size() - blobOffset !=
		        totalLineCount * sizeof(Line))
			return nullptr;

		view->mappedLines_ = reinterpret_cast<const Line*>(base + blobOffset);
		view->mapping_ = std::move(mappedFile);
		return view;
	}

	//-------------------------------------------------------------------------
	bool PdbLineTableCache::SaveToDisk(const PdbSignature& signature,
	                                   const ModuleLineTable& table) const
	{
		std::error_code error;
		std::filesystem::create_directories(diskCacheDirectory_, error);
		if (error)
			return false;

		uint64_t totalLineCount = 0;
		size_t metadataSize =
		    6 * sizeof(uint32_t) + sizeof(uint64_t) +
		    table.functions_.size() * SerializedFunctionSize;
		for (const auto& file : table.files_)
		{
			totalLineCount += file.lines_.size();
			metadataSize += SerializedFileHeaderSize +
			                file.rawName_.size() * sizeof(wchar_t);
		}
		auto blobOffset = (metadataSize + 7) & ~size_t{7};

		std::vector<char> buffer;
		buffer.reserve(blobOffset + totalLineCount * sizeof(Line));
		AppendValue(buffer, CacheMagic);
		AppendValue(buffer, CacheVersion);
		AppendValue(buffer,
		            static_cast<uint32_t>(table.hasSymbolIndexes_ ? 1 : 0));
		AppendValue(buffer, static_cast<uint32_t>(table.files_.size()));
		AppendValue(buffer, totalLineCount);
		AppendValue(buffer, static_cast<uint32_t>(table.functions_.size()));
		AppendValue(buffer, uint32_t{0}); // pad
		for (const auto& function : table.functions_)
		{
			AppendValue(buffer, function.virtualAddress_);
			AppendValue(buffer, static_cast<uint32_t>(function.length_));
			AppendValue(buffer, uint32_t{0}); // pad
		}
		uint64_t firstLine = 0;
		for (const auto& file : table.files_)
		{
			AppendValue(buffer,
			            static_cast<uint32_t>(file.rawName_.size()));
			AppendValue(buffer, static_cast<uint32_t>(file.lines_.size()));
			AppendValue(buffer, firstLine);
			Append(buffer,
			       file.rawName_.data(),
			       file.rawName_.size() * sizeof(wchar_t));
			firstLine += file.lines_.size();
		}
		buffer.resize(blobOffset, 0); // zero-pad to the aligned blob start
		for (const auto& file : table.files_)
		{
			Append(buffer,
			       file.lines_.data(),
			       file.lines_.size() * sizeof(Line));
		}

		// Write to a process-unique temporary and publish with a rename, so
		// a concurrent run sees either the old file or the complete new one.
		auto finalPath =
		    GetCacheFilePath(signature, table.hasSymbolIndexes_);
		auto tempPath = finalPath;
		tempPath += std::format(L".{}.tmp", GetCurrentProcessId());

		{
			std::ofstream ofs{tempPath,
			                  std::ios::binary | std::ios::trunc};
			if (!ofs)
				return false;
			ofs.write(buffer.data(), buffer.size());
			if (!ofs)
			{
				ofs.close();
				std::filesystem::remove(tempPath, error);
				return false;
			}
		}

		std::filesystem::rename(tempPath, finalPath, error);
		if (error)
		{
			std::filesystem::remove(tempPath, error);
			return false;
		}

		PruneDiskCache();
		return true;
	}

	//-------------------------------------------------------------------------
	void PdbLineTableCache::PruneDiskCache() const
	{
		std::error_code error;
		std::vector<
		    std::pair<std::filesystem::file_time_type, std::filesystem::path>>
		    entries;

		for (std::filesystem::directory_iterator
		         it{diskCacheDirectory_, error},
		     end;
		     !error && it != end;
		     it.increment(error))
		{
			// Reap orphaned temporaries left by a process that died between
			// write and rename (named "<cache>.<pid>.tmp"). Age-gate so a
			// live writer's in-progress temp is never removed.
			if (it->path().extension() == TempFileExtension)
			{
				std::error_code timeError;
				auto tempWriteTime = it->last_write_time(timeError);
				if (!timeError &&
				    std::filesystem::file_time_type::clock::now() -
				            tempWriteTime >
				        std::chrono::minutes{10})
					std::filesystem::remove(it->path(), timeError);
				continue;
			}
			if (it->path().extension() != CacheFileExtension)
				continue;
			auto writeTime = it->last_write_time(error);
			if (!error)
				entries.emplace_back(writeTime, it->path());
		}

		if (entries.size() <= MaxDiskCacheEntries)
			return;

		std::sort(entries.begin(), entries.end());
		auto removeCount = entries.size() - MaxDiskCacheEntries;
		for (size_t i = 0; i < removeCount; ++i)
			std::filesystem::remove(entries[i].second, error);
	}
}
