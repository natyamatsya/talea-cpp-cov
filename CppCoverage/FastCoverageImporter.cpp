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
#include "FastCoverageImporter.hpp"

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <map>
#include <sstream>
#include <unordered_map>

#include "tools/Log.hpp"

#include "Address.hpp"
#include "CppCoverageException.hpp"
#include "CoverageFilterManager.hpp"
#include "CoverageFilterSettings.hpp"
#include "DebugInformationEnumerator.hpp"
#include "ExecutedAddressManager.hpp"

#include "FileFilter/ModuleInfo.hpp"
#include "FileFilter/FileInfo.hpp"
#include "FileFilter/LineInfo.hpp"

#include "CoverageRuntime/FastCovFormat.hpp"

namespace CppCoverage
{
	namespace
	{
		//---------------------------------------------------------------------
		struct BasicBlock
		{
			std::uint32_t rva_;
			unsigned int count_;
		};

		//---------------------------------------------------------------------
		struct LineRecord
		{
			std::uint32_t rva_;
			std::wstring filePath_;
			unsigned int lineNumber_;
		};

		//---------------------------------------------------------------------
		// Collects every selected line record (with its rva) and the function
		// ranges (with demangled names) of one module from its PDB.
		class ModuleCollector : public IDebugInformationHandler
		{
		  public:
			ModuleCollector(ICoverageFilterManager& filterManager,
			                const std::filesystem::path& modulePath)
			    : filterManager_{filterManager}, modulePath_{modulePath}
			{
			}

			bool IsSourceFileSelected(
			    const std::filesystem::path& path) override
			{
				return filterManager_.IsSourceFileSelected(path.wstring());
			}

			void OnModuleFunctions(
			    const std::vector<FunctionRange>& functions) override
			{
				functions_ = functions;
			}

			void OnSourceFile(const std::filesystem::path& path,
			                  const std::vector<Line>& lines) override
			{
				std::vector<FileFilter::LineInfo> lineInfos;
				lineInfos.reserve(lines.size());
				for (const auto& line : lines)
					lineInfos.emplace_back(line.lineNumber_,
					                       line.virtualAddress_,
					                       line.symbolIndex_);

				FileFilter::FileInfo fileInfo{path, std::move(lineInfos)};
				FileFilter::ModuleInfo moduleInfo{nullptr, modulePath_,
				                                  nullptr};

				const auto& collection = fileInfo.lineInfoColllection_;
				for (const auto& lineInfo : collection)
				{
					if (!filterManager_.IsLineSelected(
					        moduleInfo, fileInfo, lineInfo))
						continue;
					records_.push_back(LineRecord{
					    static_cast<std::uint32_t>(lineInfo.virtualAddress_),
					    path.wstring(),
					    static_cast<unsigned int>(lineInfo.lineNumber_)});
				}
			}

			std::vector<FunctionRange>& GetFunctions() { return functions_; }
			std::vector<LineRecord>& GetLineRecords() { return records_; }

		  private:
			ICoverageFilterManager& filterManager_;
			const std::filesystem::path modulePath_;
			std::vector<FunctionRange> functions_;
			std::vector<LineRecord> records_;
		};

		//---------------------------------------------------------------------
		// Execution count of the basic block that covers rva: the block with
		// the greatest start address not past rva. 0 when rva precedes every
		// block.
		unsigned int CountAt(const std::vector<BasicBlock>& blocks,
		                     std::uint32_t rva)
		{
			auto it = std::upper_bound(
			    blocks.begin(),
			    blocks.end(),
			    rva,
			    [](std::uint32_t value, const BasicBlock& block) {
				    return value < block.rva_;
			    });
			if (it == blocks.begin())
				return 0;
			return (it - 1)->count_;
		}

		//---------------------------------------------------------------------
		// Demangled name of the function containing rva, or empty.
		const std::wstring*
		FunctionNameAt(const std::vector<IDebugInformationHandler::FunctionRange>&
		                   functions,
		               std::uint32_t rva)
		{
			auto it = std::upper_bound(
			    functions.begin(),
			    functions.end(),
			    rva,
			    [](std::uint32_t value, const auto& function) {
				    return value <
				           static_cast<std::uint32_t>(function.virtualAddress_);
			    });
			if (it == functions.begin())
				return nullptr;
			--it;
			auto start = static_cast<std::uint32_t>(it->virtualAddress_);
			if (rva - start >= it->length_)
				return nullptr;
			return &it->name_;
		}

		//---------------------------------------------------------------------
		// The template stem of an instantiation name, or empty when the name is
		// not a template. "foo<int>" -> "foo"; "ns::v<int>::at" -> "ns::v".
		std::wstring TemplateStem(const std::wstring& name)
		{
			auto pos = name.find(L'<');
			if (pos == std::wstring::npos)
				return {};
			return name.substr(0, pos);
		}

		//---------------------------------------------------------------------
		std::vector<char> ReadFile(const std::filesystem::path& path)
		{
			std::ifstream ifs{path, std::ios::binary};
			if (!ifs)
				return {};
			return std::vector<char>{std::istreambuf_iterator<char>{ifs},
			                         std::istreambuf_iterator<char>{}};
		}
	}

	//-------------------------------------------------------------------------
	FastCoverageImporter::FastCoverageImporter(
	    const CoverageFilterSettings& coverageFilterSettings,
	    const std::vector<UnifiedDiffSettings>& unifiedDiffSettings,
	    const std::vector<std::wstring>& excludedLineRegexes,
	    const std::vector<SubstitutePdbSourcePath>& substitutePdbSourcePaths)
	    : coverageFilterSettings_{coverageFilterSettings},
	      unifiedDiffSettings_{unifiedDiffSettings},
	      excludedLineRegexes_{excludedLineRegexes},
	      substitutePdbSourcePaths_{substitutePdbSourcePaths}
	{
	}

	//-------------------------------------------------------------------------
	FastCoverageImporter::Result
	FastCoverageImporter::Import(const std::filesystem::path& dumpPath)
	{
		auto bytes = ReadFile(dumpPath);
		if (bytes.size() < sizeof(FastCov::DumpHeader))
			THROW(L"Fast coverage dump is truncated: " + dumpPath.wstring());

		const auto* header =
		    reinterpret_cast<const FastCov::DumpHeader*>(bytes.data());
		if (header->magic_ != FastCov::DumpMagic ||
		    header->version_ != FastCov::DumpVersion)
			THROW(L"Not a fast coverage dump: " + dumpPath.wstring());

		auto expected =
		    sizeof(FastCov::DumpHeader) +
		    static_cast<std::size_t>(header->moduleCount_) *
		        sizeof(FastCov::ModuleEntry) +
		    static_cast<std::size_t>(header->basicBlockCount_) *
		        sizeof(FastCov::BasicBlockRecord);
		if (bytes.size() < expected)
			THROW(L"Fast coverage dump is truncated: " + dumpPath.wstring());

		const auto* modules = reinterpret_cast<const FastCov::ModuleEntry*>(
		    bytes.data() + sizeof(FastCov::DumpHeader));
		const auto* records =
		    reinterpret_cast<const FastCov::BasicBlockRecord*>(
		        bytes.data() + sizeof(FastCov::DumpHeader) +
		        static_cast<std::size_t>(header->moduleCount_) *
		            sizeof(FastCov::ModuleEntry));

		// Group basic blocks by module.
		std::vector<std::vector<BasicBlock>> blocksByModule(
		    header->moduleCount_);
		for (std::uint32_t i = 0; i < header->basicBlockCount_; ++i)
		{
			const auto& record = records[i];
			if (record.moduleIndex_ < header->moduleCount_)
				blocksByModule[record.moduleIndex_].push_back(
				    BasicBlock{record.rva_, record.count_});
		}

		CoverageFilterManager filterManager{coverageFilterSettings_,
		                                     unifiedDiffSettings_,
		                                     excludedLineRegexes_,
		                                     /*useReleaseCoverageFilter*/ false};

		ExecutedAddressManager manager;
		std::wstring coverageName;

		// stem -> instantiation name -> (file, line) -> count.
		std::map<std::wstring,
		         std::map<std::wstring,
		                  std::pair<std::filesystem::path,
		                            std::map<unsigned int, unsigned int>>>>
		    templates;

		for (std::uint32_t m = 0; m < header->moduleCount_; ++m)
		{
			auto& blocks = blocksByModule[m];
			if (blocks.empty())
				continue;

			std::filesystem::path modulePath{modules[m].path_};
			if (!filterManager.IsModuleSelected(modulePath.wstring()))
				continue;

			// A fresh enumeration (no cache) so function names are resolved.
			DebugInformationEnumerator enumerator{
			    substitutePdbSourcePaths_,
			    /*needSymbolIndex*/ false,
			    /*lineTableCache*/ nullptr,
			    /*needFunctionNames*/ true};
			ModuleCollector collector{filterManager, modulePath};
			if (!enumerator.Enumerate(modulePath, collector))
			{
				LOG_WARNING << L"Fast coverage: no debug information for "
				            << modulePath.wstring();
				continue;
			}

			std::sort(blocks.begin(),
			          blocks.end(),
			          [](const auto& lhs, const auto& rhs) {
				          return lhs.rva_ < rhs.rva_;
			          });

			if (coverageName.empty())
				coverageName = modulePath.filename().wstring();

			auto moduleTag = reinterpret_cast<HANDLE>(
			    static_cast<std::uintptr_t>(m) + 1);
			manager.AddModule(modulePath.wstring(), nullptr);

			for (const auto& record : collector.GetLineRecords())
			{
				auto count = CountAt(blocks, record.rva_);

				Address address{moduleTag,
				                reinterpret_cast<void*>(
				                    static_cast<std::uintptr_t>(record.rva_))};
				manager.RegisterAddress(
				    address, record.filePath_, record.lineNumber_, 0);
				if (count > 0)
					manager.MarkAddressAsExecuted(address);

				// Attribute the line to its instantiation for the template axis.
				const auto* functionName =
				    FunctionNameAt(collector.GetFunctions(), record.rva_);
				if (functionName && !functionName->empty())
				{
					auto stem = TemplateStem(*functionName);
					if (!stem.empty())
					{
						auto& instantiation =
						    templates[stem][*functionName];
						instantiation.first = record.filePath_;
						auto& lineCount =
						    instantiation.second[record.lineNumber_];
						lineCount = std::max(lineCount, count);
					}
				}
			}
		}

		Result result{manager.CreateCoverageData(coverageName, 0), {}};

		for (const auto& [stem, instantiations] : templates)
		{
			TemplateReport report;
			report.stem_ = stem;
			for (const auto& [name, fileAndLines] : instantiations)
			{
				Instantiation instantiation;
				instantiation.name_ = name;
				instantiation.file_ = fileAndLines.first;
				for (const auto& [lineNumber, count] : fileAndLines.second)
					instantiation.lines_.push_back(
					    InstantiationLine{lineNumber, count});
				report.instantiations_.push_back(std::move(instantiation));
			}
			result.templates_.push_back(std::move(report));
		}

		return result;
	}

	//-------------------------------------------------------------------------
	std::wstring FastCoverageImporter::FormatTemplateReport(
	    const std::vector<TemplateReport>& templates)
	{
		std::wostringstream ostr;
		ostr << L"# Template instantiation coverage\n\n";
		if (templates.empty())
		{
			ostr << L"No template instantiations found.\n";
			return ostr.str();
		}

		for (const auto& report : templates)
		{
			ostr << L"## " << report.stem_ << L"\n\n";

			// The union of lines across instantiations, so the table shows
			// which lines ran for which instantiation.
			std::map<unsigned int, bool> allLines;
			for (const auto& instantiation : report.instantiations_)
				for (const auto& line : instantiation.lines_)
					allLines[line.lineNumber_];

			for (const auto& instantiation : report.instantiations_)
			{
				std::map<unsigned int, unsigned int> byLine;
				for (const auto& line : instantiation.lines_)
					byLine[line.lineNumber_] = line.count_;

				std::size_t covered = 0;
				for (const auto& [lineNumber, count] : byLine)
					if (count > 0)
						++covered;

				ostr << L"- `" << instantiation.name_ << L"` — "
				     << covered << L"/" << byLine.size() << L" lines"
				     << L" (" << instantiation.file_.filename().wstring()
				     << L")\n";
			}

			// Lines covered by some but not all instantiations are the gaps a
			// plain per-line boolean hides.
			ostr << L"\n  Lines covered in some but not all instantiations:";
			bool anyGap = false;
			for (const auto& [lineNumber, unused] : allLines)
			{
				std::size_t coveredIn = 0;
				std::size_t presentIn = 0;
				for (const auto& instantiation : report.instantiations_)
				{
					for (const auto& line : instantiation.lines_)
					{
						if (line.lineNumber_ != lineNumber)
							continue;
						++presentIn;
						if (line.count_ > 0)
							++coveredIn;
					}
				}
				if (presentIn > 0 && coveredIn > 0 && coveredIn < presentIn)
				{
					ostr << L" " << lineNumber;
					anyGap = true;
				}
			}
			ostr << (anyGap ? L"\n\n" : L" (none)\n\n");
		}

		return ostr.str();
	}
}
