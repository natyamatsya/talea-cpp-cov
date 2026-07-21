// OpenCppCoverage is an open source code coverage for C++.
// Copyright (C) 2017 OpenCppCoverage
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

#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "CppCoverageExport.hpp"
#include "SubstitutePdbSourcePath.hpp"

struct IDiaSession;
struct IDiaLineNumber;
struct IDiaSourceFile;
struct IDiaEnumLineNumbers;

namespace CppCoverage
{
	//-------------------------------------------------------------------------
	class IDebugInformationHandler
	{
	  public:
		struct Line
		{
			Line(unsigned long lineNumber,
			     int64_t virtualAddress,
			     unsigned long symbolIndex,
			     unsigned long length)
			    : lineNumber_{lineNumber},
			      virtualAddress_{virtualAddress},
			      symbolIndex_{symbolIndex},
			      length_{length}
			{
			}
			Line(const Line&) = default;

			Line& operator=(const Line&) = default;

			unsigned long lineNumber_;
			unsigned long symbolIndex_;
			int64_t virtualAddress_;
			unsigned long length_;
		};

		// A function's code range, from the PDB function symbols. Straight
		// -line run analysis never crosses these boundaries. name_ is the
		// demangled symbol (e.g. "foo<int>"), populated only when the
		// enumerator is asked for function names (the template/instantiation
		// axis); it is empty otherwise and never cached.
		struct FunctionRange
		{
			int64_t virtualAddress_;
			unsigned long length_;
			std::wstring name_;
		};

		virtual ~IDebugInformationHandler() = default;
		virtual bool IsSourceFileSelected(const std::filesystem::path&) = 0;
		virtual void OnSourceFile(const std::filesystem::path&,
		                          const std::vector<Line>&) = 0;

		// Called once per module, before the source files, with the function
		// ranges sorted by address. Optional: the default ignores them.
		virtual void OnModuleFunctions(const std::vector<FunctionRange>&)
		{
		}
	};

	struct ModuleLineTable;
	class PdbLineTableCache;

	//-------------------------------------------------------------------------
	class CPPCOVERAGE_DLL DebugInformationEnumerator
	{
	  public:
		// needSymbolIndex controls whether the (expensive) DIA symbol index
		// is resolved per line record. It is only consumed by the optimized
		// build filter, so it defaults to off.
		//
		// When a PdbLineTableCache is given, the DIA parse of a module is
		// skipped whenever the cache already holds a line table for the
		// module's PDB signature.
		explicit DebugInformationEnumerator(
		    const std::vector<SubstitutePdbSourcePath>&,
		    bool needSymbolIndex = false,
		    std::shared_ptr<PdbLineTableCache> lineTableCache = nullptr,
		    bool needFunctionNames = false);
		bool Enumerate(const std::filesystem::path&,
		               IDebugInformationHandler&);

	  private:
		std::shared_ptr<ModuleLineTable>
		ParseModule(const std::filesystem::path&) const;

		void OnNewLine(IDiaSession&,
		               IDiaLineNumber&,
		               std::vector<IDebugInformationHandler::Line>&) const;

		std::filesystem::path
		RemapSourceFileName(const std::wstring& rawName) const;

		const std::vector<SubstitutePdbSourcePath> substitutePdbSourcePaths_;
		const bool needSymbolIndex_;
		const std::shared_ptr<PdbLineTableCache> lineTableCache_;
		const bool needFunctionNames_;
	};
}
