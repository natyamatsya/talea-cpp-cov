// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 natyamatsya and talea-cpp-cov contributors

#pragma once
#include <cstdint>

#include <string>
#include <map>
#include <unordered_map>

#include <Windows.h>
#include <optional>

#include "coverage/CoverageData.hpp"
#include "BranchInstruction.hpp"
#include "Address.hpp"
#include "TaleaCppCovExport.hpp"

#include "tools/Error.hpp"

namespace TaleaCppCov
{
	class BreakPoint;

	// Tracks conditional branch instructions (Jcc) and their outcomes.
	//
	// A breakpoint is set on each monitored Jcc. When it fires, the branch
	// outcome (taken or fall through) is computed from EFLAGS, the original
	// instruction is restored and the thread single-steps over it (trap
	// flag). On the resulting single-step event, the breakpoint is set
	// again unless both outcomes have been observed.
	class TALEACPPCOV_DLL BranchCoverageManager
	{
	public:
		BranchCoverageManager();
		~BranchCoverageManager();

		void AddModule(const std::wstring& moduleName, void* dllBaseOfImage);

		[[nodiscard]] Tools::Expected<void>
		RegisterBranch(const Address&,
		               const std::wstring& filename,
		               std::uint32_t lineNumber,
		               BranchCondition,
		               std::uint8_t originalInstruction);

		bool IsMonitoredBranch(const Address&) const;
		std::optional<std::uint8_t> GetOriginalInstruction(const Address&) const;

		[[nodiscard]] Tools::Expected<void>
		OnBreakPointHit(const Address&, HANDLE hThread, const BreakPoint&);
		bool OnSingleStep(HANDLE hProcess, HANDLE hThread, const BreakPoint&);

		void OnExitProcess(HANDLE hProcess);
		void OnExitThread(DWORD threadId);
		void OnUnloadModule(HANDLE hProcess, void* dllBaseOfImage);

		void DecorateCoverageData(Coverage::CoverageData&) const;

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

		std::map<std::wstring, Module> modules_;
		std::unordered_map<Address, BranchState, AddressHasher> branches_;
		std::unordered_map<DWORD, std::pair<HANDLE, void*>> pendingReArms_;
		LastModule lastModule_;
	};
}
