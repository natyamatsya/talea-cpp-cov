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
#include <cstdint>
#include <cstring>
#include <fstream>

#include <mio/mmap.hpp>

namespace CppCoverage
{
	namespace
	{
		// Cache file layout (little-endian, no alignment):
		//   u32 magic, u32 version, u8 hasSymbolIndexes, u32 fileCount,
		//   then per file: u32 nameChars, wchar_t[nameChars],
		//                  u32 lineCount,
		//                  then per line: u32 lineNumber, u32 symbolIndex,
		//                                 i64 virtualAddress, u32 length,
		//   then u32 functionCount,
		//   then per function: i64 virtualAddress, u32 length.
		constexpr uint32_t CacheMagic = 0x4350434F; // "OCPC"
		constexpr uint32_t CacheVersion = 2;
		constexpr auto CacheFileExtension = L".lines";
		constexpr size_t SerializedLineSize =
		    3 * sizeof(uint32_t) + sizeof(int64_t);
		constexpr size_t SerializedFunctionSize =
		    sizeof(int64_t) + sizeof(uint32_t);

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
		return base / L"OpenCppCoverage" / L"PdbCache";
	}

	//-------------------------------------------------------------------------
	std::shared_ptr<const ModuleLineTable>
	PdbLineTableCache::TryGet(const PdbSignature& signature,
	                          bool needSymbolIndexes)
	{
		std::lock_guard<std::mutex> lock(mutex_);
		auto key = signature.ToString();
		auto it = tables_.find(key);
		if (it != tables_.end() &&
		    (it->second->hasSymbolIndexes_ || !needSymbolIndexes))
		{
			++memoryHitCount_;
			return it->second;
		}

		if (!diskCacheDirectory_.empty())
		{
			auto table = TryLoadFromDisk(GetCacheFilePath(signature, true));
			if (!table && !needSymbolIndexes)
				table =
				    TryLoadFromDisk(GetCacheFilePath(signature, false));
			if (table && (table->hasSymbolIndexes_ || !needSymbolIndexes))
			{
				++diskHitCount_;
				tables_[key] = table;
				return table;
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
		auto it = tables_.find(key);

		// Never replace a table carrying symbol indexes with one without.
		if (it == tables_.end() || !it->second->hasSymbolIndexes_ ||
		    table->hasSymbolIndexes_)
			tables_[key] = table;

		if (!diskCacheDirectory_.empty())
			SaveToDisk(signature, *table);
	}

	//-------------------------------------------------------------------------
	int PdbLineTableCache::GetMemoryHitCount() const
	{
		std::lock_guard<std::mutex> lock(mutex_);
		return memoryHitCount_;
	}

	//-------------------------------------------------------------------------
	int PdbLineTableCache::GetDiskHitCount() const
	{
		std::lock_guard<std::mutex> lock(mutex_);
		return diskHitCount_;
	}

	//-------------------------------------------------------------------------
	int PdbLineTableCache::GetMissCount() const
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
	std::shared_ptr<const ModuleLineTable> PdbLineTableCache::TryLoadFromDisk(
	    const std::filesystem::path& path) const
	{
		std::error_code error;
		auto mappedFile = mio::make_mmap_source(path.wstring(), error);
		if (error || mappedFile.empty())
			return nullptr;

		Reader reader{mappedFile.data(),
		              mappedFile.data() + mappedFile.size()};
		if (reader.Read<uint32_t>() != CacheMagic ||
		    reader.Read<uint32_t>() != CacheVersion)
			return nullptr;

		auto table = std::make_shared<ModuleLineTable>();
		table->hasSymbolIndexes_ = reader.Read<uint8_t>() != 0;

		// Counts are validated against the remaining bytes before any
		// reserve, so a corrupt file cannot request absurd allocations.
		auto fileCount = reader.Read<uint32_t>();
		if (reader.failed_ ||
		    fileCount > reader.Remaining() / (2 * sizeof(uint32_t)))
			return nullptr;
		table->files_.reserve(fileCount);

		for (uint32_t fileIndex = 0; fileIndex < fileCount; ++fileIndex)
		{
			ModuleLineTable::File file;

			auto nameChars = reader.Read<uint32_t>();
			if (reader.failed_ ||
			    nameChars > reader.Remaining() / sizeof(wchar_t))
				return nullptr;
			file.rawName_.resize(nameChars);
			reader.ReadInto(file.rawName_.data(),
			                nameChars * sizeof(wchar_t));

			auto lineCount = reader.Read<uint32_t>();
			if (reader.failed_ ||
			    lineCount > reader.Remaining() / SerializedLineSize)
				return nullptr;
			file.lines_.reserve(lineCount);

			for (uint32_t lineIndex = 0; lineIndex < lineCount;
			     ++lineIndex)
			{
				auto lineNumber = reader.Read<uint32_t>();
				auto symbolIndex = reader.Read<uint32_t>();
				auto virtualAddress = reader.Read<int64_t>();
				auto length = reader.Read<uint32_t>();
				file.lines_.emplace_back(
				    lineNumber, virtualAddress, symbolIndex, length);
			}

			table->files_.push_back(std::move(file));
		}

		auto functionCount = reader.Read<uint32_t>();
		if (reader.failed_ ||
		    functionCount > reader.Remaining() / SerializedFunctionSize)
			return nullptr;
		table->functions_.reserve(functionCount);
		for (uint32_t functionIndex = 0; functionIndex < functionCount;
		     ++functionIndex)
		{
			auto virtualAddress = reader.Read<int64_t>();
			auto length = reader.Read<uint32_t>();
			table->functions_.push_back(
			    {virtualAddress, static_cast<unsigned long>(length)});
		}

		if (reader.failed_ || reader.Remaining() != 0)
			return nullptr;

		return table;
	}

	//-------------------------------------------------------------------------
	void PdbLineTableCache::SaveToDisk(const PdbSignature& signature,
	                                   const ModuleLineTable& table) const
	{
		std::error_code error;
		std::filesystem::create_directories(diskCacheDirectory_, error);
		if (error)
			return;

		size_t totalSize = 4 * sizeof(uint32_t) + sizeof(uint8_t) +
		                   table.functions_.size() * SerializedFunctionSize;
		for (const auto& file : table.files_)
		{
			totalSize += 2 * sizeof(uint32_t) +
			             file.rawName_.size() * sizeof(wchar_t) +
			             file.lines_.size() * SerializedLineSize;
		}

		std::vector<char> buffer;
		buffer.reserve(totalSize);
		AppendValue(buffer, CacheMagic);
		AppendValue(buffer, CacheVersion);
		AppendValue(buffer,
		            static_cast<uint8_t>(table.hasSymbolIndexes_ ? 1 : 0));
		AppendValue(buffer, static_cast<uint32_t>(table.files_.size()));
		for (const auto& file : table.files_)
		{
			AppendValue(buffer,
			            static_cast<uint32_t>(file.rawName_.size()));
			Append(buffer,
			       file.rawName_.data(),
			       file.rawName_.size() * sizeof(wchar_t));
			AppendValue(buffer, static_cast<uint32_t>(file.lines_.size()));
			for (const auto& line : file.lines_)
			{
				AppendValue(buffer,
				            static_cast<uint32_t>(line.lineNumber_));
				AppendValue(buffer,
				            static_cast<uint32_t>(line.symbolIndex_));
				AppendValue(buffer,
				            static_cast<int64_t>(line.virtualAddress_));
				AppendValue(buffer, static_cast<uint32_t>(line.length_));
			}
		}
		AppendValue(buffer, static_cast<uint32_t>(table.functions_.size()));
		for (const auto& function : table.functions_)
		{
			AppendValue(buffer, function.virtualAddress_);
			AppendValue(buffer,
			            static_cast<uint32_t>(function.length_));
		}

		// Write to a process-unique temporary and publish with a rename, so
		// a concurrent run sees either the old file or the complete new one.
		auto finalPath =
		    GetCacheFilePath(signature, table.hasSymbolIndexes_);
		auto tempPath = finalPath;
		tempPath += L"." + std::to_wstring(GetCurrentProcessId()) + L".tmp";

		{
			std::ofstream ofs{tempPath,
			                  std::ios::binary | std::ios::trunc};
			if (!ofs)
				return;
			ofs.write(buffer.data(), buffer.size());
			if (!ofs)
			{
				ofs.close();
				std::filesystem::remove(tempPath, error);
				return;
			}
		}

		std::filesystem::rename(tempPath, finalPath, error);
		if (error)
		{
			std::filesystem::remove(tempPath, error);
			return;
		}

		PruneDiskCache();
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
