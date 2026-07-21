// OpenCppCoverage is an open source code coverage for C++.
// Copyright (C) 2014 OpenCppCoverage
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
#include "ColumnarCoverage.hpp"

#include <cstring>
#include <fstream>

#include "Plugin/Exporter/CoverageData.hpp"
#include "Plugin/Exporter/ModuleCoverage.hpp"
#include "Plugin/Exporter/FileCoverage.hpp"
#include "Plugin/Exporter/LineCoverage.hpp"

#include "Tools/Tool.hpp"

#include "../ExporterException.hpp"

namespace Exporter
{
	namespace
	{
		//---------------------------------------------------------------------
		// Buffered little-endian writer.
		class Writer
		{
		public:
			explicit Writer(std::vector<char>& buffer) : buffer_(buffer) {}

			void U8(std::uint8_t v) { buffer_.push_back(static_cast<char>(v)); }

			void U32(std::uint32_t v)
			{
				for (int i = 0; i < 4; ++i)
					U8(static_cast<std::uint8_t>(v >> (8 * i)));
			}

			void I32(std::int32_t v) { U32(static_cast<std::uint32_t>(v)); }

			void Varint(std::uint64_t v)
			{
				while (v >= 0x80)
				{
					U8(static_cast<std::uint8_t>(v) | 0x80);
					v >>= 7;
				}
				U8(static_cast<std::uint8_t>(v));
			}

			void Str(const std::string& s)
			{
				U32(static_cast<std::uint32_t>(s.size()));
				buffer_.insert(buffer_.end(), s.begin(), s.end());
			}

			void Bytes(const std::uint8_t* p, size_t n)
			{
				buffer_.insert(buffer_.end(),
				               reinterpret_cast<const char*>(p),
				               reinterpret_cast<const char*>(p) + n);
			}

		private:
			std::vector<char>& buffer_;
		};

		//---------------------------------------------------------------------
		// Bounds-checked little-endian reader over an in-memory buffer.
		class Reader
		{
		public:
			Reader(const char* data, size_t size)
			    : p_(reinterpret_cast<const std::uint8_t*>(data)), end_(p_ + size)
			{
			}

			std::uint8_t U8()
			{
				if (p_ >= end_)
					THROW(L"Truncated columnar coverage file.");
				return *p_++;
			}

			std::uint32_t U32()
			{
				std::uint32_t v = 0;
				for (int i = 0; i < 4; ++i)
					v |= static_cast<std::uint32_t>(U8()) << (8 * i);
				return v;
			}

			std::int32_t I32() { return static_cast<std::int32_t>(U32()); }

			std::uint64_t Varint()
			{
				std::uint64_t v = 0;
				int shift = 0;
				for (;;)
				{
					std::uint8_t b = U8();
					v |= static_cast<std::uint64_t>(b & 0x7F) << shift;
					if (!(b & 0x80))
						return v;
					shift += 7;
					if (shift >= 64)
						THROW(L"Malformed varint in columnar coverage file.");
				}
			}

			std::string Str()
			{
				auto n = U32();
				if (static_cast<size_t>(end_ - p_) < n)
					THROW(L"Truncated string in columnar coverage file.");
				std::string s(reinterpret_cast<const char*>(p_), n);
				p_ += n;
				return s;
			}

			void ReadBitmap(std::vector<std::uint8_t>& executed, size_t lineCount)
			{
				size_t byteCount = (lineCount + 7) / 8;
				if (static_cast<size_t>(end_ - p_) < byteCount)
					THROW(L"Truncated bitmap in columnar coverage file.");
				executed.resize(lineCount);
				for (size_t i = 0; i < lineCount; ++i)
					executed[i] = (p_[i / 8] >> (i % 8)) & 1;
				p_ += byteCount;
			}

		private:
			const std::uint8_t* p_;
			const std::uint8_t* end_;
		};

		//---------------------------------------------------------------------
		std::uint8_t BranchFlags(bool taken, bool fallenThrough)
		{
			return static_cast<std::uint8_t>((taken ? 1 : 0) | (fallenThrough ? 2 : 0));
		}

		//---------------------------------------------------------------------
		// Reads one file's columns from the stream (magic/header already
		// consumed) and hands them to sink(modulePath, filePath, FileData&&).
		template <typename FileSink>
		void ParseModules(
		    Reader& reader,
		    const std::vector<std::string>& strings,
		    FileSink&& sink)
		{
			auto lookup = [&strings](std::uint32_t id) -> std::filesystem::path {
				if (id >= strings.size())
					THROW(L"Bad string id in columnar coverage file.");
				return Tools::Utf8ToWString(strings[id]);
			};

			auto moduleCount = reader.U32();
			for (std::uint32_t m = 0; m < moduleCount; ++m)
			{
				auto modulePath = lookup(reader.U32());
				auto fileCount = reader.U32();
				for (std::uint32_t f = 0; f < fileCount; ++f)
				{
					auto filePath = lookup(reader.U32());
					auto lineCount = reader.U32();

					ColumnarBinary::FileData data;
					data.lines.reserve(lineCount);
					unsigned int line = 0;
					for (std::uint32_t i = 0; i < lineCount; ++i)
					{
						line += static_cast<unsigned int>(reader.Varint());
						data.lines.push_back(line);
					}
					reader.ReadBitmap(data.executed, lineCount);

					auto branchLineCount = reader.U32();
					for (std::uint32_t b = 0; b < branchLineCount; ++b)
					{
						auto branchLine = static_cast<unsigned int>(reader.Varint());
						auto branchCount = reader.U32();
						auto& branchMap = data.branches[branchLine];
						for (std::uint32_t k = 0; k < branchCount; ++k)
						{
							auto index = static_cast<unsigned int>(reader.Varint());
							branchMap[index] = reader.U8();
						}
					}
					sink(modulePath, filePath, std::move(data));
				}
			}
		}
	}

	//-------------------------------------------------------------------------
	namespace ColumnarBinary
	{
		//---------------------------------------------------------------------
		void Write(const Plugin::CoverageData& coverageData, std::ostream& ostr)
		{
			// Intern every module and file path into a string table so repeated
			// paths cost 4 bytes each in the module section.
			std::vector<std::string> strings;
			std::map<std::string, std::uint32_t> stringIds;
			auto intern = [&](const std::filesystem::path& path) -> std::uint32_t {
				auto utf8 = Tools::ToUtf8String(path.wstring());
				auto it = stringIds.find(utf8);
				if (it != stringIds.end())
					return it->second;
				auto id = static_cast<std::uint32_t>(strings.size());
				strings.push_back(utf8);
				stringIds.emplace(std::move(utf8), id);
				return id;
			};

			// First pass: assign ids in module/file order (also fixes the id of
			// each path used below).
			for (const auto& module : coverageData.GetModules())
			{
				intern(module->GetPath());
				for (const auto& file : module->GetFiles())
					intern(file->GetPath());
			}

			std::vector<char> buffer;
			Writer w(buffer);
			w.U32(Magic);
			w.U32(Version);
			w.Str(Tools::ToUtf8String(coverageData.GetName()));
			w.I32(coverageData.GetExitCode());

			w.U32(static_cast<std::uint32_t>(strings.size()));
			for (const auto& s : strings)
				w.Str(s);

			w.U32(static_cast<std::uint32_t>(coverageData.GetModules().size()));
			for (const auto& module : coverageData.GetModules())
			{
				w.U32(stringIds.at(Tools::ToUtf8String(module->GetPath().wstring())));
				w.U32(static_cast<std::uint32_t>(module->GetFiles().size()));

				for (const auto& file : module->GetFiles())
				{
					w.U32(stringIds.at(
					    Tools::ToUtf8String(file->GetPath().wstring())));

					const auto& lineCollection = file->GetLineCollection();
					w.U32(static_cast<std::uint32_t>(lineCollection.size()));

					// Delta-varint line numbers + packed executed bitmap.
					std::vector<std::uint8_t> bitmap(
					    (lineCollection.size() + 7) / 8, 0);
					unsigned int prev = 0;
					size_t i = 0;
					for (const auto& [lineNumber, line] : lineCollection)
					{
						w.Varint(lineNumber - prev);
						prev = lineNumber;
						if (line.HasBeenExecuted())
							bitmap[i / 8] |= static_cast<std::uint8_t>(1 << (i % 8));
						++i;
					}
					w.Bytes(bitmap.data(), bitmap.size());

					// Sparse branch side-table.
					std::uint32_t branchLineCount = 0;
					for (const auto& [lineNumber, line] : lineCollection)
						if (!line.GetBranches().empty())
							++branchLineCount;
					w.U32(branchLineCount);
					for (const auto& [lineNumber, line] : lineCollection)
					{
						const auto& branches = line.GetBranches();
						if (branches.empty())
							continue;
						w.Varint(lineNumber);
						w.U32(static_cast<std::uint32_t>(branches.size()));
						for (const auto& branch : branches)
						{
							w.Varint(branch.GetBranchIndex());
							w.U8(BranchFlags(branch.HasBeenTaken(),
							                 branch.HasBeenFallenThrough()));
						}
					}
				}
			}

			ostr.write(buffer.data(), static_cast<std::streamsize>(buffer.size()));
			if (!ostr)
				THROW(L"Cannot write columnar coverage file.");
		}

		//---------------------------------------------------------------------
		Plugin::CoverageData Read(const std::filesystem::path& path)
		{
			auto buffer = ReadFile(path);
			Reader reader(buffer.data(), buffer.size());
			if (reader.U32() != Magic)
				THROW(L"Not a columnar coverage file: " + path.wstring());
			reader.U32(); // version
			auto name = Tools::Utf8ToWString(reader.Str());
			auto exitCode = reader.I32();

			auto stringCount = reader.U32();
			std::vector<std::string> strings;
			strings.reserve(stringCount);
			for (std::uint32_t i = 0; i < stringCount; ++i)
				strings.push_back(reader.Str());

			Plugin::CoverageData coverageData{name, exitCode};

			// Files of a module are contiguous, so tracking the last module
			// path rebuilds the stored order without sorting.
			Plugin::ModuleCoverage* currentModule = nullptr;
			std::filesystem::path currentModulePath;
			bool hasModule = false;

			ParseModules(
			    reader, strings,
			    [&](const std::filesystem::path& modulePath,
			        const std::filesystem::path& filePath,
			        FileData&& data) {
				    if (!hasModule || currentModulePath != modulePath)
				    {
					    currentModule = &coverageData.AddModule(modulePath);
					    currentModulePath = modulePath;
					    hasModule = true;
				    }
				    auto& file = currentModule->AddFile(filePath);
				    for (size_t i = 0; i < data.lines.size(); ++i)
					    file.AddLine(data.lines[i], data.executed[i] != 0);
				    for (const auto& [lineNumber, branchMap] : data.branches)
					    for (const auto& [index, flags] : branchMap)
						    file.AddBranch(
						        lineNumber,
						        Plugin::BranchCoverage{index,
						                               (flags & 1) != 0,
						                               (flags & 2) != 0});
			    });
			return coverageData;
		}

		//---------------------------------------------------------------------
		bool IsColumnarFile(const std::filesystem::path& path)
		{
			std::ifstream ifs(path.string(), std::ios::binary);
			if (!ifs)
				return false;
			char header[4] = {};
			ifs.read(header, 4);
			if (ifs.gcount() != 4)
				return false;
			std::uint32_t magic = 0;
			for (int i = 0; i < 4; ++i)
				magic |= static_cast<std::uint32_t>(
				             static_cast<std::uint8_t>(header[i]))
				         << (8 * i);
			return magic == Magic;
		}

		//---------------------------------------------------------------------
		std::vector<char> ReadFile(const std::filesystem::path& path)
		{
			std::ifstream ifs(path.string(), std::ios::binary | std::ios::ate);
			if (!ifs)
				THROW(L"Cannot open file " + path.wstring());
			auto size = ifs.tellg();
			ifs.seekg(0);
			std::vector<char> buffer(static_cast<size_t>(size));
			if (size > 0 && !ifs.read(buffer.data(), size))
				THROW(L"Cannot read file " + path.wstring());
			return buffer;
		}
	}

	//-------------------------------------------------------------------------
	void ColumnarCoverageMerger::MergeFile(
	    const std::filesystem::path& modulePath,
	    const std::filesystem::path& filePath,
	    ColumnarBinary::FileData&& incoming)
	{
		auto& existing = modules_[modulePath][filePath];
		if (existing.lines.empty())
		{
			existing = std::move(incoming);
			return;
		}

		if (existing.lines == incoming.lines)
		{
			// Same build => identical line array => straight bitwise OR.
			for (size_t i = 0; i < existing.executed.size(); ++i)
				existing.executed[i] |= incoming.executed[i];
		}
		else
		{
			// Different builds: sorted union of the two line arrays.
			std::vector<unsigned int> lines;
			std::vector<std::uint8_t> executed;
			lines.reserve(existing.lines.size() + incoming.lines.size());
			executed.reserve(lines.capacity());
			size_t a = 0, b = 0;
			while (a < existing.lines.size() && b < incoming.lines.size())
			{
				if (existing.lines[a] < incoming.lines[b])
				{
					lines.push_back(existing.lines[a]);
					executed.push_back(existing.executed[a]);
					++a;
				}
				else if (incoming.lines[b] < existing.lines[a])
				{
					lines.push_back(incoming.lines[b]);
					executed.push_back(incoming.executed[b]);
					++b;
				}
				else
				{
					lines.push_back(existing.lines[a]);
					executed.push_back(existing.executed[a] | incoming.executed[b]);
					++a;
					++b;
				}
			}
			for (; a < existing.lines.size(); ++a)
			{
				lines.push_back(existing.lines[a]);
				executed.push_back(existing.executed[a]);
			}
			for (; b < incoming.lines.size(); ++b)
			{
				lines.push_back(incoming.lines[b]);
				executed.push_back(incoming.executed[b]);
			}
			existing.lines = std::move(lines);
			existing.executed = std::move(executed);
		}

		// Branches are sparse; OR the flags per (line, branchIndex).
		for (auto& [lineNumber, branchMap] : incoming.branches)
		{
			auto& target = existing.branches[lineNumber];
			for (const auto& [index, flags] : branchMap)
				target[index] |= flags;
		}
	}

	//-------------------------------------------------------------------------
	void ColumnarCoverageMerger::Add(const Plugin::CoverageData& coverageData)
	{
		name_ = coverageData.GetName();
		if (coverageData.GetExitCode())
			lastNotZeroExitCode_ = coverageData.GetExitCode();

		for (const auto& module : coverageData.GetModules())
		{
			for (const auto& file : module->GetFiles())
			{
				const auto& lineCollection = file->GetLineCollection();
				ColumnarBinary::FileData data;
				data.lines.reserve(lineCollection.size());
				data.executed.reserve(lineCollection.size());
				for (const auto& [lineNumber, line] : lineCollection)
				{
					data.lines.push_back(lineNumber);
					data.executed.push_back(line.HasBeenExecuted() ? 1 : 0);
					const auto& branches = line.GetBranches();
					if (!branches.empty())
					{
						auto& branchMap = data.branches[lineNumber];
						for (const auto& branch : branches)
							branchMap[branch.GetBranchIndex()] = BranchFlags(
							    branch.HasBeenTaken(),
							    branch.HasBeenFallenThrough());
					}
				}
				MergeFile(module->GetPath(), file->GetPath(), std::move(data));
			}
		}
	}

	//-------------------------------------------------------------------------
	bool ColumnarCoverageMerger::AddColumnarFile(const std::filesystem::path& path)
	{
		if (!ColumnarBinary::IsColumnarFile(path))
			return false;

		auto buffer = ColumnarBinary::ReadFile(path);
		Reader reader(buffer.data(), buffer.size());
		if (reader.U32() != ColumnarBinary::Magic)
			return false;
		reader.U32(); // version
		auto name = reader.Str();
		auto exitCode = reader.I32();

		name_ = Tools::Utf8ToWString(name);
		if (exitCode)
			lastNotZeroExitCode_ = exitCode;

		auto stringCount = reader.U32();
		std::vector<std::string> strings;
		strings.reserve(stringCount);
		for (std::uint32_t i = 0; i < stringCount; ++i)
			strings.push_back(reader.Str());

		ParseModules(reader, strings,
		             [this](const std::filesystem::path& modulePath,
		                    const std::filesystem::path& filePath,
		                    ColumnarBinary::FileData&& data) {
			             MergeFile(modulePath, filePath, std::move(data));
		             });
		return true;
	}

	//-------------------------------------------------------------------------
	Plugin::CoverageData ColumnarCoverageMerger::ToCoverageData() const
	{
		Plugin::CoverageData coverageData{name_, lastNotZeroExitCode_};

		for (const auto& [modulePath, files] : modules_)
		{
			auto& module = coverageData.AddModule(modulePath);
			for (const auto& [filePath, data] : files)
			{
				auto& file = module.AddFile(filePath);
				for (size_t i = 0; i < data.lines.size(); ++i)
					file.AddLine(data.lines[i], data.executed[i] != 0);

				for (const auto& [lineNumber, branchMap] : data.branches)
				{
					for (const auto& [index, flags] : branchMap)
						file.AddBranch(
						    lineNumber,
						    Plugin::BranchCoverage{index,
						                           (flags & 1) != 0,
						                           (flags & 2) != 0});
				}
			}
		}
		return coverageData;
	}
}
