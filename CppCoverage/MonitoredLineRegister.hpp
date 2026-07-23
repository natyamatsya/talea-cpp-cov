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
#include <cstdint>

#include "DebugInformationEnumerator.hpp"
#include "BranchInstruction.hpp"
#include "Tools/Error.hpp"
#include <memory>
#include <optional>
#include <unordered_map>
#include <filesystem>

namespace FileFilter
{
	class LineInfo;
	class ModuleInfo;
}

namespace CppCoverage
{
	class ICoverageFilterManager;
	class BreakPoint;
	class Address;
	class ExecutedAddressManager;
	class FilterAssistant;
	class BranchCoverageManager;
	class BranchDisassembler;

	class MonitoredLineRegister : private IDebugInformationHandler
	{
	  public:
		MonitoredLineRegister(std::shared_ptr<BreakPoint>,
		                      std::shared_ptr<ExecutedAddressManager>,
		                      std::shared_ptr<ICoverageFilterManager>,
		                      std::unique_ptr<DebugInformationEnumerator>,
		                      std::shared_ptr<FilterAssistant>,
		                      std::shared_ptr<BranchCoverageManager> = nullptr,
		                      bool enableLineMerging = false);
		~MonitoredLineRegister();

		[[nodiscard]] Tools::Expected<bool>
		RegisterLineToMonitor(const std::filesystem::path& modulePath,
		                      HANDLE hProcess,
		                      void* baseOfImage);

	  private:
		bool IsSourceFileSelected(const std::filesystem::path&) override;
		void OnSourceFile(const std::filesystem::path&,
		                  const std::vector<Line>&) override;
		void OnModuleFunctions(
		    const std::vector<FunctionRange>&) override;

		// The byte read from memory at an already-armed branch breakpoint is
		// the 0xCC, not the real instruction; returns the branch manager's
		// recorded original byte in that case, otherwise oldInstruction.
		std::uint8_t ResolveOriginalInstruction(
		    const Address&, std::uint8_t oldInstruction) const;

		using LineNumberByAddress =
		    std::unordered_map<DWORD64, std::vector<std::int32_t>>;
		[[nodiscard]] Tools::Expected<void>
		SetBreakPoint(const std::filesystem::path&,
		              HANDLE hProcess,
		              std::vector<DWORD64>&&,
		              const LineNumberByAddress&);

		struct SelectedLine
		{
			std::uint32_t lineNumber_;
			DWORD64 startAddress_;
			std::uint32_t length_;
		};
		[[nodiscard]] Tools::Expected<void>
		RegisterBranches(const std::filesystem::path&,
		                 HANDLE hProcess,
		                 const std::vector<SelectedLine>&);

		// The validated conditional branches of the function containing
		// lineAddress, or nullptr when there is no enclosing function or its
		// code range was rejected (inline jump-table data, unreadable or
		// oversized). Scanned once per function and cached for the module.
		const std::vector<BranchInstruction>*
		GetFunctionBranches(HANDLE hProcess, DWORD64 lineAddress);

		[[nodiscard]] Tools::Expected<void> RegisterPendingLines();
		void RestoreOriginalBytes(HANDLE hProcess,
		                          DWORD64 startAddress,
		                          std::vector<std::uint8_t>& buffer) const;

		const FileFilter::ModuleInfo& GetModuleInfo() const;

		std::unique_ptr<FileFilter::ModuleInfo> moduleInfo_;
		// Set by a source-file callback whose breakpoint/branch registration
		// failed; RegisterLineToMonitor reports it after Enumerate returns
		// (the IDebugInformationHandler callbacks cannot return an error).
		std::optional<Tools::Error> error_;
		const std::shared_ptr<BreakPoint> breakPoint_;
		const std::shared_ptr<ExecutedAddressManager> executedAddressManager_;
		const std::shared_ptr<ICoverageFilterManager> coverageFilterManager_;
		const std::unique_ptr<DebugInformationEnumerator>
		    debugInformationEnumerator_;
		const std::shared_ptr<FilterAssistant> filterAssistant_;
		const std::shared_ptr<BranchCoverageManager> branchCoverageManager_;
		std::unique_ptr<BranchDisassembler> branchDisassembler_;

		// Straight-line run merging: selected lines are collected per module
		// and registered afterwards with one breakpoint per run leader.
		const bool enableLineMerging_;
		bool is64BitModule_ = true;
		struct PendingLine
		{
			DWORD64 address_;
			std::uint32_t length_;
			std::uint32_t lineNumber_;
			size_t fileIndex_;
		};
		std::vector<PendingLine> pendingLines_;
		std::vector<std::filesystem::path> pendingFiles_;
		std::vector<FunctionRange> functionRanges_;

		// function start address -> its validated branches; nullptr = rejected.
		std::unordered_map<DWORD64,
		                   std::shared_ptr<const std::vector<BranchInstruction>>>
		    functionBranchCache_;
	};
}
