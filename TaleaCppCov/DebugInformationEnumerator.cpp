//// OpenCppCoverage is an open source code coverage for C++.
//// Copyright (C) 2017 OpenCppCoverage
////
//// This program is free software: you can redistribute it and/or modify
//// it under the terms of the GNU General Public License as published by
//// the Free Software Foundation, either version 3 of the License, or
//// any later version.
////
//// This program is distributed in the hope that it will be useful,
//// but WITHOUT ANY WARRANTY; without even the implied warranty of
//// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//// GNU General Public License for more details.
////
//// You should have received a copy of the GNU General Public License
//// along with this program.  If not, see <http://www.gnu.org/licenses/>.

#include "stdafx.h"
#include <cstdint>
#include "DebugInformationEnumerator.hpp"

#include <Windows.h>
#include <diacreate.h>
#include <dia2.h>
#include <atlbase.h>

#include <algorithm>
#include <filesystem>
#include <memory>
#include <optional>
#include <unordered_map>
#include "Tools/StringUtils.hpp"

#include "Tools/Log.hpp"

#include "TaleaCppCovException.hpp"
#include "PdbLineTableCache.hpp"
#include "PdbSignature.hpp"
#include "RawPdbReader.hpp"

namespace TaleaCppCov
{
	namespace
	{
		//----------------------------------------------------------------------
		class DiaString
		{
		  public:
			DiaString() = default;
			DiaString(const DiaString&) = delete;
			DiaString(DiaString&&) = delete;

			DiaString& operator=(const DiaString&) = delete;
			DiaString& operator=(DiaString&&) = delete;

			~DiaString()
			{
				ReleaseMemory();
			}

			BSTR* operator&()
			{
				ReleaseMemory();
				return &str_;
			}

			operator std::wstring() const
			{
				if (!str_)
					return L"";
				return str_;
			}

		  private:
			void ReleaseMemory()
			{
				if (str_ != nullptr)
				{
					// Cannot call SysFreeString as dia generated invalid BSTR
					// for x64.
					LocalFree(str_ - 2);
					str_ = nullptr;
				}
			}

			BSTR str_ = nullptr;
		};

		//----------------------------------------------------------------------
		template <typename Value, typename Collection, typename Fct>
		void EnumerateCollection(Collection& collection, Fct fct)
		{
			CComPtr<Value> value;
			ULONG celtFetched;
			while (collection.Next(1, &value, &celtFetched) == S_OK &&
			       celtFetched == 1 && value != nullptr)
			{
				fct(*value);
				value = nullptr;
			}
		}

		//----------------------------------------------------------------------
		Tools::Expected<CComPtr<IDiaEnumSourceFiles>>
		GetEnumSourceFiles(IDiaSession& session)
		{
			CComPtr<IDiaEnumTables> tables;
			if (session.getEnumTables(&tables) != S_OK || !tables)
				return std::unexpected(Tools::Error{L"DIA: Cannot get tables"});

			CComPtr<IDiaEnumSourceFiles> sourceFiles;

			EnumerateCollection<IDiaTable>(*tables, [&](IDiaTable& table) {
				if (!sourceFiles)
				{
					CComPtr<IDiaEnumSourceFiles> currentSourceFiles;
					if (table.QueryInterface(_uuidof(IDiaEnumSourceFiles),
					                         (void**)&currentSourceFiles) ==
					    S_OK)
					{
						sourceFiles = currentSourceFiles;
					}
				}
			});

			return sourceFiles;
		}

		//----------------------------------------------------------------------
		Tools::Expected<std::wstring>
		GetRawSourceFileName(IDiaSourceFile& sourceFile)
		{
			DiaString fileName;
			if (sourceFile.get_fileName(&fileName) != S_OK)
				return std::unexpected(Tools::Error{L"DIA: Cannot get filename"});
			return std::wstring{fileName};
		}

		//----------------------------------------------------------------------
		Tools::Expected<CComPtr<IDiaEnumLineNumbers>>
		GetAllLineNumbers(IDiaSession& session)
		{
			// A single query over the whole image returns every line record
			// in one pass. The session load address defaults to 0, so line
			// records report RVAs directly.
			CComPtr<IDiaEnumLineNumbers> lineNumbers;
			if (session.findLinesByRVA(0, 0xFFFFFFFF, &lineNumbers) != S_OK ||
			    !lineNumbers)
			{
				return std::unexpected(Tools::Error{L"DIA: Cannot find lines"});
			}

			return lineNumbers;
		}

		//----------------------------------------------------------------------
		struct DiaLoadCallback : public IDiaLoadCallback
		{
			//------------------------------------------------------------------
			HRESULT STDMETHODCALLTYPE QueryInterface(
			    REFIID riid,
			    _COM_Outptr_ void __RPC_FAR* __RPC_FAR* ppvObject) override
			{
				if (!ppvObject)
					return E_POINTER;

				if (riid == IID_IDiaLoadCallback)
				{
					*ppvObject = this;
					return S_OK;
				}

				return E_NOINTERFACE;
			}

			//------------------------------------------------------------------
			ULONG STDMETHODCALLTYPE AddRef() override
			{
				return 1; // On stack
			}

			//------------------------------------------------------------------
			ULONG STDMETHODCALLTYPE Release() override
			{
				return 1; // On stack
			}

			//------------------------------------------------------------------
			HRESULT STDMETHODCALLTYPE NotifyDebugDir(BOOL fExecutable,
			                                         DWORD cbData,
			                                         BYTE* pbData) override
			{
				return S_OK;
			}

			//------------------------------------------------------------------
			HRESULT STDMETHODCALLTYPE NotifyOpenDBG(LPCOLESTR dbgPath,
			                                        HRESULT resultCode) override
			{
				return S_OK;
			}

			//------------------------------------------------------------------
			HRESULT STDMETHODCALLTYPE NotifyOpenPDB(LPCOLESTR pdbPath,
			                                        HRESULT resultCode) override
			{
				LOG_DEBUG("Try to load pdb from {}: {}", pdbPath, (resultCode == S_OK ? "Success" : "Failed"));
				return S_OK;
			}

			//------------------------------------------------------------------
			HRESULT STDMETHODCALLTYPE RestrictRegistryAccess() override
			{
				return S_OK;
			}

			//------------------------------------------------------------------
			HRESULT STDMETHODCALLTYPE RestrictSymbolServerAccess() override
			{
				return S_OK;
			}
		};

		//----------------------------------------------------------------------
		// A null value means "no DIA data for this module" (treated as no debug
		// info); an unexpected means DIA itself could not be loaded.
		Tools::Expected<CComPtr<IDiaDataSource>>
		LoadDataForExe(const std::filesystem::path& path)
		{
			CComPtr<IDiaDataSource> sourcePtr;

			auto msDia = L"msdia140.dll";
			if (NoRegCoCreate(msDia,
			                  _uuidof(DiaSourceAlt),
			                  _uuidof(IDiaDataSource),
			                  (void**)&sourcePtr) != S_OK)
			{
				return std::unexpected(Tools::Error{
				    std::wstring(L"DIA: Cannot register dia. Make sure ") +
				    msDia + L" is in the current directory."});
			}

			DiaLoadCallback diaLoadCallback;
			if (!sourcePtr ||
			    sourcePtr->loadDataForExe(
			        path.wstring().c_str(), nullptr, &diaLoadCallback) != S_OK)
			{
				return CComPtr<IDiaDataSource>{nullptr};
			}
			return sourcePtr;
		}
	}

	//--------------------------------------------------------------------------
	DebugInformationEnumerator::DebugInformationEnumerator(
	    const std::vector<SubstitutePdbSourcePath>& substitutePdbSourcePaths,
	    bool needSymbolIndex,
	    std::shared_ptr<PdbLineTableCache> lineTableCache,
	    bool needFunctionNames,
	    bool useRawPdb,
	    bool rawPdbOnly)
		: substitutePdbSourcePaths_{ substitutePdbSourcePaths }
		, needSymbolIndex_{ needSymbolIndex }
		, lineTableCache_{ std::move(lineTableCache) }
		, needFunctionNames_{ needFunctionNames }
		, useRawPdb_{ useRawPdb }
		, rawPdbOnly_{ rawPdbOnly }
	{
	}

	//-------------------------------------------------------------------------
	Tools::Expected<bool>
	DebugInformationEnumerator::Enumerate(const std::filesystem::path& path,
	                                      IDebugInformationHandler& handler)
	{
		// The DIA parse (ParseModule) reports failure by value. The remaining
		// throwers reached from here are the handler callbacks (the FileFilter
		// layer, not yet no-throw); catch everything around them so nothing
		// escapes the std::execution::par preparse and calls std::terminate.
		try
		{
			// The cache is keyed by the PDB's GUID + age (the linker's exact
			// identity), read from the PE debug directory without loading DIA.
			std::optional<PdbSignature> signature;
			std::shared_ptr<const LineTableView> lineTable;

			if (lineTableCache_)
			{
				signature = ExtractPdbSignature(path);
				if (signature)
					lineTable =
					    lineTableCache_->TryGet(*signature, needSymbolIndex_);
			}

			if (!lineTable)
			{
				auto parsedTable = ParseModule(path);
				if (!parsedTable)
					return std::unexpected(std::move(parsedTable.error()));
				if (!*parsedTable)
					return false; // no debug info
				if (lineTableCache_ && signature)
					lineTableCache_->Store(*signature, *parsedTable);
				lineTable = std::make_shared<LineTableView>(
				    std::move(*parsedTable));
			}

			handler.OnModuleFunctions(lineTable->Functions());

			// Path substitution and source-file selection run on every replay:
			// they depend on the run's settings, so a cached table stays valid
			// whatever filters are active. A mapped view decodes one file's
			// lines at a time into the scratch vector, so the whole table never
			// becomes heap-resident (MEMORY_ROADMAP.md, Tier 1).
			std::vector<IDebugInformationHandler::Line> scratch;
			for (std::size_t i = 0; i < lineTable->FileCount(); ++i)
			{
				auto sourcePath = RemapSourceFileName(lineTable->FileRawName(i));
				if (handler.IsSourceFileSelected(sourcePath))
					handler.OnSourceFile(sourcePath,
					                     lineTable->FileLines(i, scratch));
			}

			return true;
		}
		catch (const std::exception& e)
		{
			return std::unexpected(Tools::ErrorFromException(e));
		}
		catch (...)
		{
			return std::unexpected(Tools::Error{
			    L"Unknown error reading debug information for " + path.wstring()});
		}
	}

	//-------------------------------------------------------------------------
	Tools::Expected<std::shared_ptr<ModuleLineTable>>
	DebugInformationEnumerator::ParseModule(
	    const std::filesystem::path& path) const
	{
		// Fast path: read the line table straight from the PDB with raw_pdb
		// (mmap + columnar scan, parsed in parallel) instead of DIA's per-record
		// COM calls. It provides line records, function ranges and - from the
		// proc records - function names, but not symbol indexes, so the
		// optimized-build filter (needSymbolIndex_) still uses DIA; everything
		// else falls back to DIA only when raw_pdb cannot handle the PDB
		// (missing, FASTLINK, mismatched).
		if (useRawPdb_ && !needSymbolIndex_)
		{
			auto lineTable = std::make_shared<ModuleLineTable>();
			if (RawPdbReader::TryRead(path, *lineTable))
				return lineTable;
			// The parallel pre-parse must not touch COM-bound DIA from a worker
			// thread, so skip the module rather than fall back; the on-load
			// parse (serialized in the debug loop) handles it later.
			if (rawPdbOnly_)
				return std::shared_ptr<ModuleLineTable>{nullptr};
		}

		auto sourceResult = LoadDataForExe(path);
		if (!sourceResult)
			return std::unexpected(std::move(sourceResult.error()));
		auto& sourcePtr = *sourceResult;
		if (!sourcePtr)
			return std::shared_ptr<ModuleLineTable>{nullptr};

		CComPtr<IDiaSession> sessionPtr;
		if (sourcePtr->openSession(&sessionPtr) != S_OK || !sessionPtr)
			return std::unexpected(Tools::Error{L"DIA: Cannot open session."});

		auto lineTable = std::make_shared<ModuleLineTable>();
		lineTable->hasSymbolIndexes_ = needSymbolIndex_;

		// Pass 1: enumerate the source files once, in delivery order.
		// Previously the line data was re-queried once per source file via
		// get_compilands + findLines, paying a fixed per-call cost for every
		// header — O(files) findLines calls that dominate startup on large
		// translation units where many headers share one compiland.
		//
		// Files are keyed by their raw PDB name rather than
		// IDiaSourceFile::get_uniqueId because DIA assigns different unique
		// ids to the source-file objects reached through the source-files
		// table (pass 1) and through a line record's get_sourceFile
		// (pass 2); the name is stable across both paths.
		std::unordered_map<std::wstring, size_t> fileIndexByName;
		std::optional<Tools::Error> error;

		auto sourceFilesResult = GetEnumSourceFiles(*sessionPtr);
		if (!sourceFilesResult)
			return std::unexpected(std::move(sourceFilesResult.error()));
		auto& sourceFiles = *sourceFilesResult;
		if (!sourceFiles)
			return std::unexpected(Tools::Error{L"DIA: cannot get SourceFiles"});

		EnumerateCollection<IDiaSourceFile>(
		    *sourceFiles, [&](IDiaSourceFile& sourceFile) {
			    if (error)
				    return;
			    auto nameResult = GetRawSourceFileName(sourceFile);
			    if (!nameResult)
			    {
				    error = std::move(nameResult.error());
				    return;
			    }
			    auto name = std::move(*nameResult);
			    if (fileIndexByName
			            .emplace(name, lineTable->files_.size())
			            .second)
			    {
				    lineTable->files_.push_back(
				        ModuleLineTable::File{std::move(name), {}});
			    }
		    });
		if (error)
			return std::unexpected(std::move(*error));

		// Pass 2: enumerate every line record in the module in one query and
		// bucket the records by source file. All files are collected — not
		// only the currently selected ones — so the table can be cached and
		// replayed under any later selection.
		auto lineNumbersResult = GetAllLineNumbers(*sessionPtr);
		if (!lineNumbersResult)
			return std::unexpected(std::move(lineNumbersResult.error()));
		auto& lineNumbers = *lineNumbersResult;

		// Records arrive in RVA order, so consecutive records usually share
		// a source file: cache the last resolved file to avoid re-reading
		// the source file name per record.
		std::wstring lastName;
		bool hasLastFile = false;
		ModuleLineTable::File* lastFile = nullptr;

		EnumerateCollection<IDiaLineNumber>(
		    *lineNumbers, [&](IDiaLineNumber& lineNumber) {
			    if (error)
				    return;
			    CComPtr<IDiaSourceFile> sourceFile;
			    if (lineNumber.get_sourceFile(&sourceFile) != S_OK ||
			        !sourceFile)
			    {
				    error = Tools::Error{L"DIA: Cannot get line source file"};
				    return;
			    }

			    auto nameResult = GetRawSourceFileName(*sourceFile);
			    if (!nameResult)
			    {
				    error = std::move(nameResult.error());
				    return;
			    }
			    auto name = std::move(*nameResult);

			    if (!hasLastFile || name != lastName)
			    {
				    auto it = fileIndexByName.find(name);
				    lastFile = (it != fileIndexByName.end())
				                   ? &lineTable->files_[it->second]
				                   : nullptr;
				    lastName = std::move(name);
				    hasLastFile = true;
			    }

			    if (lastFile)
			    {
				    auto lineResult =
				        OnNewLine(*sessionPtr, lineNumber, lastFile->lines_);
				    if (!lineResult)
				    {
					    error = std::move(lineResult.error());
					    return;
				    }
			    }
		    });
		if (error)
			return std::unexpected(std::move(*error));

		// Function code ranges bound the straight-line run analysis (a run
		// must never span two functions: indirect calls and exception
		// dispatch can enter any function at its entry point).
		CComPtr<IDiaSymbol> globalScope;
		if (sessionPtr->get_globalScope(&globalScope) == S_OK && globalScope)
		{
			CComPtr<IDiaEnumSymbols> functionSymbols;
			if (globalScope->findChildren(SymTagFunction,
			                              nullptr,
			                              nsNone,
			                              &functionSymbols) == S_OK &&
			    functionSymbols)
			{
				EnumerateCollection<IDiaSymbol>(
				    *functionSymbols, [&](IDiaSymbol& symbol) {
					    ULONGLONG virtualAddress = 0;
					    ULONGLONG length = 0;
					    if (symbol.get_virtualAddress(&virtualAddress) !=
					            S_OK ||
					        symbol.get_length(&length) != S_OK ||
					        length == 0)
					    {
						    return;
					    }

					    // The demangled name (e.g. "foo<std::int32_t>") is the
					    // template-instantiation identity, resolved only
					    // when a caller asks for it.
					    std::wstring name;
					    if (needFunctionNames_)
					    {
						    DiaString symbolName;
						    if (symbol.get_name(&symbolName) == S_OK)
							    name = symbolName;
					    }

					    lineTable->functions_.push_back(
					        {static_cast<int64_t>(virtualAddress),
					         static_cast<std::uint32_t>(length),
					         std::move(name)});
				    });
			}
		}
		std::sort(lineTable->functions_.begin(),
		          lineTable->functions_.end(),
		          [](const auto& lhs, const auto& rhs) {
			          return lhs.virtualAddress_ < rhs.virtualAddress_;
		          });

		return lineTable;
	}

	//----------------------------------------------------------------------
	Tools::Expected<void> DebugInformationEnumerator::OnNewLine(
	    IDiaSession& session,
	    IDiaLineNumber& lineNumber,
	    std::vector<IDebugInformationHandler::Line>& lines) const
	{
		DWORD linenum = 0;
		if (lineNumber.get_lineNumber(&linenum) != S_OK)
			return std::unexpected(Tools::Error{L"DIA: Cannot get line number"});

		// Skip invalid lines (0 is clang's "no source" marker)
		if (linenum != 0x00f00f00 && linenum != 0x00feefee && linenum != 0)
		{
			ULONGLONG virtualAddress = 0;
			if (lineNumber.get_virtualAddress(&virtualAddress) != S_OK)
				return std::unexpected(
				    Tools::Error{L"DIA: Cannot get virtual address"});

			// findSymbolByVA is a full symbol-table query per line record and
			// its result (the symbol index) is only used by the optimized
			// build filter, so skip it entirely otherwise. Kept as the DIA
			// out-parameter type (DWORD) at the boundary; narrowed to the
			// first-party std::uint32_t when the Line is built (ADR-0001).
			DWORD symIndex = 0;
			if (needSymbolIndex_)
			{
				CComPtr<IDiaSymbol> symbol;
				if (session.findSymbolByVA(virtualAddress,
				                           SymTagEnum::SymTagNull,
				                           &symbol) != S_OK ||
				    !symbol)
				{
					return std::unexpected(
					    Tools::Error{L"DIA: Cannot find symbol"});
				}

				if (symbol->get_symIndexId(&symIndex) != S_OK)
					return std::unexpected(
					    Tools::Error{L"DIA: Cannot get symIndex"});
			}

			DWORD length = 0;
			if (lineNumber.get_length(&length) != S_OK)
				return std::unexpected(
				    Tools::Error{L"DIA: Cannot get line length"});

			lines.emplace_back(static_cast<std::uint32_t>(linenum),
			                   static_cast<int64_t>(virtualAddress),
			                   static_cast<std::uint32_t>(symIndex),
			                   static_cast<std::uint32_t>(length));
		}
		return {};
	}

	//----------------------------------------------------------------------
	std::filesystem::path DebugInformationEnumerator::RemapSourceFileName(
	    const std::wstring& rawName) const
	{
		std::wstring filenameStr = rawName;

		for (const auto& paths : substitutePdbSourcePaths_)
		{
			auto pdbStartPath = paths.GetPdbStartPath().wstring();

			if (Tools::IStartsWith(filenameStr, std::wstring_view{pdbStartPath}))
			{
				auto startIndex = pdbStartPath.size();
				if (startIndex < filenameStr.size() && filenameStr[startIndex] == '\\')
					++startIndex;
				auto remainingPath = filenameStr.substr(startIndex);
				filenameStr = (paths.GetLocalPath() / remainingPath).wstring();
				break;
			}
		}

		return filenameStr;
	}
}
