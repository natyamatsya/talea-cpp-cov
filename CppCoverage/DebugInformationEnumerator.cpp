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

#include "tools/Log.hpp"

#include "CppCoverageException.hpp"
#include "PdbLineTableCache.hpp"
#include "PdbSignature.hpp"
#include "RawPdbReader.hpp"

namespace CppCoverage
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
		CComPtr<IDiaEnumSourceFiles> GetEnumSourceFiles(IDiaSession& session)
		{
			CComPtr<IDiaEnumTables> tables;
			if (session.getEnumTables(&tables) != S_OK || !tables)
				THROW("DIA: Cannot get tables");

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
		std::wstring GetRawSourceFileName(IDiaSourceFile& sourceFile)
		{
			DiaString fileName;
			if (sourceFile.get_fileName(&fileName) != S_OK)
				THROW("DIA: Cannot get filename");
			return fileName;
		}

		//----------------------------------------------------------------------
		CComPtr<IDiaEnumLineNumbers> GetAllLineNumbers(IDiaSession& session)
		{
			// A single query over the whole image returns every line record
			// in one pass. The session load address defaults to 0, so line
			// records report RVAs directly.
			CComPtr<IDiaEnumLineNumbers> lineNumbers;
			if (session.findLinesByRVA(0, 0xFFFFFFFF, &lineNumbers) != S_OK ||
			    !lineNumbers)
			{
				THROW("DIA: Cannot find lines");
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
				LOG_DEBUG << "Try to load pdb from " << pdbPath << ": "
				          << (resultCode == S_OK ? "Success" : "Failed");
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
		CComPtr<IDiaDataSource>
		LoadDataForExe(const std::filesystem::path& path)
		{
			CComPtr<IDiaDataSource> sourcePtr;

			auto msDia = L"msdia140.dll";
			if (NoRegCoCreate(msDia,
			                  _uuidof(DiaSourceAlt),
			                  _uuidof(IDiaDataSource),
			                  (void**)&sourcePtr) != S_OK)
			{
				THROW(std::wstring(L"DIA: Cannot register dia. Make sure ") +
				      msDia + L" is in the current directory.");
			}

			DiaLoadCallback diaLoadCallback;
			if (!sourcePtr ||
			    sourcePtr->loadDataForExe(
			        path.wstring().c_str(), nullptr, &diaLoadCallback) != S_OK)
			{
				return nullptr;
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
	bool
	DebugInformationEnumerator::Enumerate(const std::filesystem::path& path,
	                                      IDebugInformationHandler& handler)
	{
		// The cache is keyed by the PDB's GUID + age (the linker's exact
		// identity), read from the PE debug directory without loading DIA.
		std::optional<PdbSignature> signature;
		std::shared_ptr<const ModuleLineTable> lineTable;

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
				return false;
			if (lineTableCache_ && signature)
				lineTableCache_->Store(*signature, parsedTable);
			lineTable = std::move(parsedTable);
		}

		handler.OnModuleFunctions(lineTable->functions_);

		// Path substitution and source-file selection run on every replay:
		// they depend on the run's settings, so a cached table stays valid
		// whatever filters are active.
		for (const auto& file : lineTable->files_)
		{
			auto sourcePath = RemapSourceFileName(file.rawName_);
			if (handler.IsSourceFileSelected(sourcePath))
				handler.OnSourceFile(sourcePath, file.lines_);
		}

		return true;
	}

	//-------------------------------------------------------------------------
	std::shared_ptr<ModuleLineTable> DebugInformationEnumerator::ParseModule(
	    const std::filesystem::path& path) const
	{
		// Fast path: read the line table straight from the PDB with raw_pdb
		// (mmap + columnar scan, parsed in parallel) instead of DIA's per-record
		// COM calls. It does not provide symbol indexes or demangled function
		// names, so the optimized-build filter and the fast-mode instantiation
		// axis still use DIA; everything else falls back to DIA only when
		// raw_pdb cannot handle the PDB (missing, FASTLINK, mismatched).
		if (useRawPdb_ && !needSymbolIndex_ && !needFunctionNames_)
		{
			auto lineTable = std::make_shared<ModuleLineTable>();
			if (RawPdbReader::TryRead(path, *lineTable))
				return lineTable;
			// The parallel pre-parse must not touch COM-bound DIA from a worker
			// thread, so skip the module rather than fall back; the on-load
			// parse (serialized in the debug loop) handles it later.
			if (rawPdbOnly_)
				return nullptr;
		}

		auto sourcePtr = LoadDataForExe(path);

		if (!sourcePtr)
			return nullptr;

		CComPtr<IDiaSession> sessionPtr;
		if (sourcePtr->openSession(&sessionPtr) != S_OK || !sessionPtr)
			THROW("DIA: Cannot open session.");

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

		auto sourceFiles = GetEnumSourceFiles(*sessionPtr);
		if (!sourceFiles)
			THROW("DIA: cannot get SourceFiles");

		EnumerateCollection<IDiaSourceFile>(
		    *sourceFiles, [&](IDiaSourceFile& sourceFile) {
			    auto name = GetRawSourceFileName(sourceFile);
			    if (fileIndexByName
			            .emplace(name, lineTable->files_.size())
			            .second)
			    {
				    lineTable->files_.push_back(
				        ModuleLineTable::File{std::move(name), {}});
			    }
		    });

		// Pass 2: enumerate every line record in the module in one query and
		// bucket the records by source file. All files are collected — not
		// only the currently selected ones — so the table can be cached and
		// replayed under any later selection.
		auto lineNumbers = GetAllLineNumbers(*sessionPtr);

		// Records arrive in RVA order, so consecutive records usually share
		// a source file: cache the last resolved file to avoid re-reading
		// the source file name per record.
		std::wstring lastName;
		bool hasLastFile = false;
		ModuleLineTable::File* lastFile = nullptr;

		EnumerateCollection<IDiaLineNumber>(
		    *lineNumbers, [&](IDiaLineNumber& lineNumber) {
			    CComPtr<IDiaSourceFile> sourceFile;
			    if (lineNumber.get_sourceFile(&sourceFile) != S_OK ||
			        !sourceFile)
			    {
				    THROW("DIA: Cannot get line source file");
			    }

			    auto name = GetRawSourceFileName(*sourceFile);

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
				    OnNewLine(*sessionPtr, lineNumber, lastFile->lines_);
		    });

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

					    // The demangled name (e.g. "foo<int>") is the
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
					         static_cast<unsigned long>(length),
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
	void DebugInformationEnumerator::OnNewLine(
	    IDiaSession& session,
	    IDiaLineNumber& lineNumber,
	    std::vector<IDebugInformationHandler::Line>& lines) const
	{
		DWORD linenum = 0;
		if (lineNumber.get_lineNumber(&linenum) != S_OK)
			THROW("DIA: Cannot get line number");

		// Skip invalid lines
		if (linenum != 0x00f00f00 && linenum != 0x00feefee)
		{
			ULONGLONG virtualAddress = 0;
			if (lineNumber.get_virtualAddress(&virtualAddress) != S_OK)
				THROW("DIA: Cannot get virtual address");

			// findSymbolByVA is a full symbol-table query per line record and
			// its result (the symbol index) is only used by the optimized
			// build filter, so skip it entirely otherwise.
			unsigned long symIndex = 0;
			if (needSymbolIndex_)
			{
				CComPtr<IDiaSymbol> symbol;
				if (session.findSymbolByVA(virtualAddress,
				                           SymTagEnum::SymTagNull,
				                           &symbol) != S_OK ||
				    !symbol)
				{
					THROW("DIA: Cannot find symbol");
				}

				if (symbol->get_symIndexId(&symIndex) != S_OK)
					THROW("DIA: Cannot get symIndex");
			}

			DWORD length = 0;
			if (lineNumber.get_length(&length) != S_OK)
				THROW("DIA: Cannot get line length");

			lines.emplace_back(linenum, virtualAddress, symIndex, length);
		}
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
