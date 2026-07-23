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

#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <Windows.h>
#include <map>
#include <unordered_map>
#include <optional>

#include "Coverage/CoverageData.hpp"
#include "Address.hpp"
#include "CppCoverageExport.hpp"

namespace CppCoverage
{
	// Records which addresses cover which (module, file, line) probes and,
	// at runtime, which probes have been executed.
	//
	// Data-oriented design: probes live in a structure-of-arrays "probe
	// table" (parallel columns indexed by a dense probe id), the executed
	// state is a single contiguous byte vector (a hit bitset), file paths are
	// interned to ids, and an address fans out to probe ids (indices, stable
	// across table growth) rather than raw pointers into container nodes.
	class CPPCOVERAGE_DLL ExecutedAddressManager
	{
	public:
		ExecutedAddressManager();
		~ExecutedAddressManager();

		void AddModule(const std::wstring& moduleName, void* dllBaseOfImage);
		void OnUnloadModule(HANDLE hProcess, void* dllBaseOfImage);

		bool RegisterAddress(
			const Address&,
			const std::wstring& filename,
			std::uint32_t line,
			std::uint8_t instruction);

		std::optional<std::uint8_t> MarkAddressAsExecuted(const Address&);
		std::optional<std::uint8_t> GetOriginalInstruction(const Address&) const;

		Coverage::CoverageData CreateCoverageData(const std::wstring& name, std::int32_t exitCode) const;
		void OnExitProcess(HANDLE hProcess);

	private:
		ExecutedAddressManager(const ExecutedAddressManager&) = delete;
		ExecutedAddressManager& operator=(const ExecutedAddressManager&) = delete;

		// Identifies a probe = a unique (module, file, line) triple.
		struct ProbeKey
		{
			std::uint32_t moduleId;
			std::uint32_t fileId;
			std::uint32_t lineNumber;

			bool operator==(const ProbeKey& other) const
			{
				return moduleId == other.moduleId && fileId == other.fileId &&
				       lineNumber == other.lineNumber;
			}
		};
		struct ProbeKeyHasher
		{
			std::size_t operator()(const ProbeKey&) const noexcept;
		};

		// The addresses (per process) that reach a set of probes. Defined in
		// the .cpp so the vendored small_vector header stays out of this header.
		struct AddressData;

		std::uint32_t InternFile(const std::wstring& filename);
		std::uint32_t GetOrCreateProbe(std::uint32_t moduleId,
		                               std::uint32_t fileId,
		                               std::uint32_t lineNumber);
		template <typename Condition>
		void RemoveAddressIf(Condition);

		// Interned file paths.
		std::vector<std::wstring> filePaths_;                       // id -> path
		std::unordered_map<std::wstring, std::uint32_t> filePathIds_;

		// Modules, kept sorted by name for deterministic output.
		std::map<std::wstring, std::uint32_t> moduleIdByName_;

		// Probe table (structure of arrays), indexed by probe id.
		std::vector<std::uint32_t> probeModuleId_;
		std::vector<std::uint32_t> probeFileId_;
		std::vector<std::uint32_t> probeLineNumber_;
		std::vector<std::uint8_t> probeExecuted_;   // the hit bitset
		std::unordered_map<ProbeKey, std::uint32_t, ProbeKeyHasher> probeIdByKey_;

		// Address fan-out (per process).
		std::unordered_map<Address, AddressData, AddressHasher> addressMap_;

		// Registration context (the last added module and file).
		bool hasLastModule_ = false;
		std::uint32_t lastModuleId_ = 0;
		void* lastBaseOfImage_ = nullptr;
		bool hasLastFile_ = false;
		std::wstring lastFilename_;
		std::uint32_t lastFileId_ = 0;
	};
}
