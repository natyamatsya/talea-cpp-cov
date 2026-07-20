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

#include "stdafx.h"

#include "MonitoredLineRegister.hpp"

#include "ICoverageFilterManager.hpp"
#include "Address.hpp"
#include "BreakPoint.hpp"
#include "ExecutedAddressManager.hpp"
#include "BranchCoverageManager.hpp"
#include "BranchDisassembler.hpp"
#include "CppCoverageException.hpp"
#include "FilterAssistant.hpp"

#include "FileFilter/ModuleInfo.hpp"
#include "FileFilter/FileInfo.hpp"
#include "FileFilter/LineInfo.hpp"

#include "Tools/PEFileHeader.hpp"
#include "Tools/ProcessMemory.hpp"
#include "Tools/Log.hpp"

namespace CppCoverage
{
	namespace
	{
		struct ModuleKind : private Tools::IPEFileHeaderHandler
		{
			//----------------------------------------------------------------------------
			bool IsNativeModule(HANDLE hProcess, DWORD64 baseOfImage)
			{
				Tools::PEFileHeader fileHeader;

				fileHeader.Load(hProcess, baseOfImage, *this);
				return isNativeModule_;
			}

			//----------------------------------------------------------------------------
			bool Is64BitModule() const
			{
				return is64BitModule_;
			}

		  private:
			//-----------------------------------------------------------------
			template <typename T_IMAGE_NT_HEADERS>
			void OnNtHeader(const T_IMAGE_NT_HEADERS& ntHeaders)
			{
				const auto& optionalHeader = ntHeaders.OptionalHeader;
				auto dataDirectory =
				    optionalHeader
				        .DataDirectory[IMAGE_DIRECTORY_ENTRY_COM_DESCRIPTOR];
				isNativeModule_ = dataDirectory.VirtualAddress == 0 &&
				                  dataDirectory.Size == 0;
			}

			//-----------------------------------------------------------------
			void OnNtHeader32(HANDLE,
			                  DWORD64,
			                  const IMAGE_NT_HEADERS32& ntHeader) override
			{
				OnNtHeader(ntHeader);
				is64BitModule_ = false;
			}

			//-----------------------------------------------------------------
			void OnNtHeader64(HANDLE,
			                  DWORD64,
			                  const IMAGE_NT_HEADERS64& ntHeader) override
			{
				OnNtHeader(ntHeader);
				is64BitModule_ = true;
			}

			bool isNativeModule_ = true;
			bool is64BitModule_ = false;
		};
	}

	//----------------------------------------------------------------------------
	MonitoredLineRegister::MonitoredLineRegister(
	    std::shared_ptr<BreakPoint> breakPoint,
	    std::shared_ptr<ExecutedAddressManager> executedAddressManager,
	    std::shared_ptr<ICoverageFilterManager> coverageFilterManager,
	    std::unique_ptr<DebugInformationEnumerator> debugInformationEnumerator,
	    std::shared_ptr<FilterAssistant> filterAssistant,
	    std::shared_ptr<BranchCoverageManager> branchCoverageManager)
	    : breakPoint_{breakPoint},
	      executedAddressManager_{executedAddressManager},
	      coverageFilterManager_{coverageFilterManager},
	      debugInformationEnumerator_{std::move(debugInformationEnumerator)},
	      filterAssistant_{std::move(filterAssistant)},
	      branchCoverageManager_{std::move(branchCoverageManager)}
	{
	}

	//----------------------------------------------------------------------------
	MonitoredLineRegister::~MonitoredLineRegister() = default;

	//----------------------------------------------------------------------------
	bool MonitoredLineRegister::RegisterLineToMonitor(
	    const std::filesystem::path& modulePath,
	    HANDLE hProcess,
	    void* baseOfImage)
	{
		ModuleKind moduleKind;
		if (!moduleKind.IsNativeModule(
		        hProcess, reinterpret_cast<DWORD64>(baseOfImage)))
		{
			LOG_INFO << modulePath.wstring()
			         << " is skipped as it is a managed module.";
			return false;
		}

		executedAddressManager_->AddModule(modulePath.wstring(), baseOfImage);
		if (branchCoverageManager_)
		{
			branchCoverageManager_->AddModule(modulePath.wstring(),
			                                  baseOfImage);
			branchDisassembler_ = std::make_unique<BranchDisassembler>(
			    moduleKind.Is64BitModule());
		}

		moduleInfo_ = std::make_unique<FileFilter::ModuleInfo>(
		    hProcess, modulePath, baseOfImage);

		return debugInformationEnumerator_->Enumerate(modulePath, *this);
	}

	//--------------------------------------------------------------------------
	bool MonitoredLineRegister::IsSourceFileSelected(
	    const std::filesystem::path& path)
	{
		auto isSelected = coverageFilterManager_->IsSourceFileSelected(path.wstring());
		filterAssistant_->OnNewSourceFile(path, isSelected);
		return isSelected;
	}

	//--------------------------------------------------------------------------
	void
	MonitoredLineRegister::OnSourceFile(const std::filesystem::path& path,
	                                    const std::vector<Line>& lines)
	{
		std::vector<FileFilter::LineInfo> lineInfos;

		for (const auto& line : lines)
		{
			lineInfos.emplace_back(
			    line.lineNumber_, line.virtualAddress_, line.symbolIndex_);
		}

		FileFilter::FileInfo fileInfo{path, std::move(lineInfos)};
		const auto& moduleInfo = GetModuleInfo();

		std::vector<DWORD64> addresses;
		LineNumberByAddress lineNumberByAddress;
		std::vector<SelectedLine> selectedLines;

		const auto& lineInfoCollection = fileInfo.lineInfoColllection_;
		for (size_t index = 0; index < lineInfoCollection.size(); ++index)
		{
			const auto& lineInfo = lineInfoCollection[index];
			auto lineNumber = lineInfo.lineNumber_;
			if (coverageFilterManager_->IsLineSelected(
			        moduleInfo, fileInfo, lineInfo))
			{
				auto addressValue =
				    lineInfo.virtualAddress_ +
				    reinterpret_cast<DWORD64>(moduleInfo.baseOfImage_);

				lineNumberByAddress[addressValue].push_back(lineNumber);
				addresses.push_back(addressValue);
				if (branchCoverageManager_)
				{
					selectedLines.push_back(
					    SelectedLine{static_cast<unsigned int>(lineNumber),
					                 addressValue,
					                 lines[index].length_});
				}
			}
		}
		SetBreakPoint(path,
		              moduleInfo.hProcess_,
		              std::move(addresses),
		              lineNumberByAddress);

		if (branchCoverageManager_)
			RegisterBranches(path, moduleInfo.hProcess_, selectedLines);
	}

	//--------------------------------------------------------------------------
	void MonitoredLineRegister::SetBreakPoint(
	    const std::filesystem::path& path,
	    HANDLE hProcess,
	    std::vector<DWORD64>&& addressCollection,
	    const LineNumberByAddress& lineNumberByAddress)
	{
		auto oldInstructions =
		    breakPoint_->SetBreakPoints(hProcess, std::move(addressCollection));
		for (const auto& value : oldInstructions)
		{
			auto oldInstruction = value.first;
			const auto& addressValue = value.second;

			auto it = lineNumberByAddress.find(addressValue);
			if (it != lineNumberByAddress.end())
			{
				Address address{hProcess,
				                reinterpret_cast<void*>(addressValue)};

				// If the address is an armed branch breakpoint, the
				// instruction read from memory is the breakpoint
				// instruction itself: use the real original instruction.
				auto registeredInstruction = oldInstruction;
				if (branchCoverageManager_)
				{
					auto branchInstruction =
					    branchCoverageManager_->GetOriginalInstruction(
					        address);
					if (branchInstruction)
						registeredInstruction = *branchInstruction;
				}

				const auto& lineNumbers = it->second;
				for (auto lineNumber : lineNumbers)
				{
					if (!executedAddressManager_->RegisterAddress(
					        address,
					        path.wstring(),
					        lineNumber,
					        registeredInstruction))
					{
						breakPoint_->RemoveBreakPoint(address, oldInstruction);
					}
				}
			}
		}
	}

	//--------------------------------------------------------------------------
	void MonitoredLineRegister::RegisterBranches(
	    const std::filesystem::path& path,
	    HANDLE hProcess,
	    const std::vector<SelectedLine>& selectedLines)
	{
		struct BranchToRegister
		{
			unsigned int lineNumber_;
			BranchCondition condition_;
		};

		std::vector<DWORD64> addresses;
		std::map<DWORD64, BranchToRegister> branchByAddress;

		for (const auto& selectedLine : selectedLines)
		{
			if (selectedLine.length_ == 0)
				continue;

			auto buffer = Tools::ReadProcessMemory(
			    hProcess,
			    reinterpret_cast<void*>(selectedLine.startAddress_),
			    selectedLine.length_);

			// Restore the bytes already patched with breakpoints
			// before disassembling.
			for (size_t i = 0; i < buffer.size(); ++i)
			{
				Address address{
				    hProcess,
				    reinterpret_cast<void*>(selectedLine.startAddress_ + i)};
				auto instruction =
				    executedAddressManager_->GetOriginalInstruction(address);
				if (!instruction)
				{
					instruction =
					    branchCoverageManager_->GetOriginalInstruction(
					        address);
				}
				if (instruction)
					buffer[i] = *instruction;
			}

			auto branches = branchDisassembler_->FindConditionalBranches(
			    buffer.data(), buffer.size(), selectedLine.startAddress_);

			for (const auto& branch : branches)
			{
				Address address{
				    hProcess,
				    reinterpret_cast<void*>(branch.virtualAddress_)};

				// Shared code (e.g. headers) can be reached from several
				// source files: the first registration wins.
				if (branchCoverageManager_->IsMonitoredBranch(address))
					continue;

				auto lineInstruction =
				    executedAddressManager_->GetOriginalInstruction(address);
				if (lineInstruction)
				{
					// The address already has a line breakpoint: reuse its
					// original instruction and make sure the breakpoint is
					// armed even if it has already fired.
					branchCoverageManager_->RegisterBranch(
					    address,
					    path.wstring(),
					    selectedLine.lineNumber_,
					    branch.condition_,
					    *lineInstruction);
					breakPoint_->SetBreakPoint(address);
				}
				else if (branchByAddress
				             .emplace(branch.virtualAddress_,
				                      BranchToRegister{
				                          selectedLine.lineNumber_,
				                          branch.condition_})
				             .second)
				{
					addresses.push_back(branch.virtualAddress_);
				}
			}
		}

		auto oldInstructions =
		    breakPoint_->SetBreakPoints(hProcess, std::move(addresses));
		for (const auto& value : oldInstructions)
		{
			auto oldInstruction = value.first;
			const auto& addressValue = value.second;

			auto it = branchByAddress.find(addressValue);
			if (it == branchByAddress.end())
				continue;

			Address address{hProcess, reinterpret_cast<void*>(addressValue)};
			branchCoverageManager_->RegisterBranch(address,
			                                       path.wstring(),
			                                       it->second.lineNumber_,
			                                       it->second.condition_,
			                                       oldInstruction);
		}
	}

	//--------------------------------------------------------------------------
	const FileFilter::ModuleInfo& MonitoredLineRegister::GetModuleInfo() const
	{
		if (!moduleInfo_)
			THROW("moduleInfo_ is null.");
		return *moduleInfo_;
	}
}
