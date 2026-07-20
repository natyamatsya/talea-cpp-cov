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

#include <filesystem>
#include <unordered_map>
#include <boost/algorithm/string.hpp>

#include "tools/Log.hpp"

#include "CppCoverageException.hpp"

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
	    bool needSymbolIndex)
		: substitutePdbSourcePaths_{ substitutePdbSourcePaths }
		, needSymbolIndex_{ needSymbolIndex }
	{
	}

	//-------------------------------------------------------------------------
	bool
	DebugInformationEnumerator::Enumerate(const std::filesystem::path& path,
	                                      IDebugInformationHandler& handler)
	{
		auto sourcePtr = LoadDataForExe(path);

		if (!sourcePtr)
			return false;

		CComPtr<IDiaSession> sessionPtr;
		if (sourcePtr->openSession(&sessionPtr) != S_OK || !sessionPtr)
			THROW("DIA: Cannot open session.");

		// Pass 1: enumerate the source files once, keyed by their (remapped)
		// name, remembering the selection decision and delivery order.
		// Previously the line data was re-queried once per source file via
		// get_compilands + findLines, paying a fixed per-call cost for every
		// header — O(files) findLines calls that dominate startup on large
		// translation units where many headers share one compiland.
		//
		// The name is the key rather than IDiaSourceFile::get_uniqueId because
		// DIA assigns different unique ids to the source-file objects reached
		// through the source-files table (pass 1) and through a line record's
		// get_sourceFile (pass 2); the remapped file name is stable across
		// both paths.
		struct FileEntry
		{
			std::filesystem::path path_;
			bool selected_;
			std::vector<IDebugInformationHandler::Line> lines_;
		};
		std::unordered_map<std::wstring, FileEntry> filesByName;
		std::vector<std::wstring> orderedNames;

		auto sourceFiles = GetEnumSourceFiles(*sessionPtr);
		if (!sourceFiles)
			THROW("DIA: cannot get SourceFiles");

		EnumerateCollection<IDiaSourceFile>(
		    *sourceFiles, [&](IDiaSourceFile& sourceFile) {
			    auto filename = GetSourceFileName(sourceFile);
			    auto key = filename.wstring();
			    if (filesByName.find(key) != filesByName.end())
				    return;

			    bool selected = handler.IsSourceFileSelected(filename);
			    orderedNames.push_back(key);
			    filesByName.emplace(
			        key, FileEntry{std::move(filename), selected, {}});
		    });

		// Pass 2: enumerate every line record in the module in one query and
		// bucket the records of selected files.
		auto lineNumbers = GetAllLineNumbers(*sessionPtr);

		// Records arrive in RVA order, so consecutive records usually share a
		// source file: cache the last resolved file to avoid re-reading and
		// re-mapping the source file name per record.
		std::wstring lastName;
		bool hasLastFile = false;
		FileEntry* lastEntry = nullptr;

		EnumerateCollection<IDiaLineNumber>(
		    *lineNumbers, [&](IDiaLineNumber& lineNumber) {
			    CComPtr<IDiaSourceFile> sourceFile;
			    if (lineNumber.get_sourceFile(&sourceFile) != S_OK ||
			        !sourceFile)
			    {
				    THROW("DIA: Cannot get line source file");
			    }

			    auto name = GetSourceFileName(*sourceFile).wstring();

			    if (!hasLastFile || name != lastName)
			    {
				    auto it = filesByName.find(name);
				    lastEntry = (it != filesByName.end()) ? &it->second
				                                          : nullptr;
				    lastName = std::move(name);
				    hasLastFile = true;
			    }

			    if (lastEntry && lastEntry->selected_)
				    OnNewLine(*sessionPtr, lineNumber, lastEntry->lines_);
		    });

		// Deliver each selected file (even when empty) in source-file
		// enumeration order, matching the previous behavior.
		for (const auto& name : orderedNames)
		{
			auto& entry = filesByName[name];
			if (entry.selected_)
				handler.OnSourceFile(entry.path_, entry.lines_);
		}

		return true;
	}

	//----------------------------------------------------------------------
	void DebugInformationEnumerator::OnNewLine(
	    IDiaSession& session,
	    IDiaLineNumber& lineNumber,
	    std::vector<IDebugInformationHandler::Line>& lines)
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
	std::filesystem::path DebugInformationEnumerator::GetSourceFileName(
	    IDiaSourceFile& sourceFile) const
	{
		DiaString fileName;
		if (sourceFile.get_fileName(&fileName) != S_OK)
			THROW("DIA: Cannot get filename");
		std::wstring filenameStr = fileName;

		for (const auto& paths : substitutePdbSourcePaths_)
		{
			auto pdbStartPath = paths.GetPdbStartPath().wstring();

			if (boost::istarts_with(filenameStr, pdbStartPath))
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
