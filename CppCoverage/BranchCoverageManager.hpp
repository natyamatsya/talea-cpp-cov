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

#pragma once

#include <string>
#include <map>
#include <unordered_map>

#include <Windows.h>
#include <optional>

#include "Plugin/Exporter/CoverageData.hpp"
#include "BranchInstruction.hpp"
#include "Address.hpp"
#include "CppCoverageExport.hpp"

namespace CppCoverage
{
	class BreakPoint;

	// Tracks conditional branch instructions (Jcc) and their outcomes.
	//
	// A breakpoint is set on each monitored Jcc. When it fires, the branch
	// outcome (taken or fall through) is computed from EFLAGS, the original
	// instruction is restored and the thread single-steps over it (trap
	// flag). On the resulting single-step event, the breakpoint is set
	// again unless both outcomes have been observed.
	class CPPCOVERAGE_DLL BranchCoverageManager
	{
	public:
		BranchCoverageManager();
		~BranchCoverageManager();

		void AddModule(const std::wstring& moduleName, void* dllBaseOfImage);

		void RegisterBranch(const Address&,
		                    const std::wstring& filename,
		                    unsigned int lineNumber,
		                    BranchCondition,
		                    unsigned char originalInstruction);

		bool IsMonitoredBranch(const Address&) const;
		std::optional<unsigned char> GetOriginalInstruction(const Address&) const;

		void OnBreakPointHit(const Address&, HANDLE hThread, const BreakPoint&);
		bool OnSingleStep(HANDLE hProcess, HANDLE hThread, const BreakPoint&);

		void OnExitProcess(HANDLE hProcess);
		void OnExitThread(DWORD threadId);
		void OnUnloadModule(HANDLE hProcess, void* dllBaseOfImage);

		void DecorateCoverageData(Plugin::CoverageData&) const;

	private:
		BranchCoverageManager(const BranchCoverageManager&) = delete;
		BranchCoverageManager& operator=(const BranchCoverageManager&) = delete;

		struct BranchOutcome;
		struct BranchState;
		struct File;
		struct Module;
		struct LastModule
		{
			Module* module_;
			void* baseOfImage_;
		};

		Module& GetLastAddedModule();

		std::map<std::wstring, Module> modules_;
		std::unordered_map<Address, BranchState, AddressHasher> branches_;
		std::unordered_map<DWORD, std::pair<HANDLE, void*>> pendingReArms_;
		LastModule lastModule_;
	};
}
