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

#include "stdafx.h"
#include <cstdint>
#include "BreakPoint.hpp"

#include "TaleaCppCovException.hpp" // TaleaCppCov::GetErrorMessage
#include "Address.hpp"

#include "Tools/Log.hpp"
#include "Tools/ProcessMemory.hpp"

namespace TaleaCppCov
{
	using Addresses = std::vector<DWORD64>;
	using AddressesIt = Addresses::const_iterator;

	//-------------------------------------------------------------------------
	Tools::Expected<void>
	SetBreakPointsRange(HANDLE hProcess,
	                    AddressesIt begin,
	                    AddressesIt end,
	                    BreakPoint::InstructionCollection& oldInstructions)
	{
		if (begin == end)
			return {};

		auto firstValue = *begin;
		auto memorySpaceSize =
		    *(end - 1) - firstValue + sizeof(BreakPoint::breakPointInstruction);
		auto firstAddress = reinterpret_cast<void*>(firstValue);
		auto buffer = Tools::ReadProcessMemory(
		    hProcess, firstAddress, static_cast<size_t>(memorySpaceSize));
		if (!buffer)
			return std::unexpected(buffer.error());

		for (auto it = begin; it < end; ++it)
		{
			auto index = static_cast<size_t>(*it - firstValue);
			auto oldInstruction = (*buffer)[index];
			(*buffer)[index] = BreakPoint::breakPointInstruction;
			oldInstructions.emplace_back(oldInstruction, *it);
		}
		return Tools::WriteProcessMemory(
		    hProcess, firstAddress, &(*buffer)[0], buffer->size());
	}

	const std::uint8_t BreakPoint::breakPointInstruction = 0xCC;

	//-------------------------------------------------------------------------
	Tools::Expected<BreakPoint::InstructionCollection>
	BreakPoint::SetBreakPoints(HANDLE hProcess, Addresses&& addresses) const
	{
		InstructionCollection oldInstructions;

		std::sort(addresses.begin(), addresses.end());
		auto beginRange = addresses.cbegin();

		for (auto it = beginRange; it < addresses.cend(); ++it)
		{
			if (*it - *beginRange > 4096)
			{
				if (auto result = SetBreakPointsRange(
				        hProcess, beginRange, it, oldInstructions);
				    !result)
					return std::unexpected(result.error());
				beginRange = it;
			}
		}
		if (auto result = SetBreakPointsRange(
		        hProcess, beginRange, addresses.end(), oldInstructions);
		    !result)
			return std::unexpected(result.error());

		return oldInstructions;
	}

	//-------------------------------------------------------------------------
	Tools::Expected<void>
	BreakPoint::RemoveBreakPoint(const Address& address,
	                             std::uint8_t oldInstruction) const
	{
		return Tools::WriteProcessMemory(address.GetProcessHandle(),
		                                 address.GetValue(),
		                                 &oldInstruction,
		                                 sizeof(oldInstruction));
	}

	//-------------------------------------------------------------------------
	Tools::Expected<void> BreakPoint::SetBreakPoint(const Address& address) const
	{
		auto instruction = breakPointInstruction;
		return Tools::WriteProcessMemory(address.GetProcessHandle(),
		                                 address.GetValue(),
		                                 &instruction,
		                                 sizeof(instruction));
	}

	//-------------------------------------------------------------------------
	Tools::Expected<void>
	BreakPoint::RewindInstructionPointer(HANDLE hThread, bool setTrapFlag) const
	{
		CONTEXT lcContext;
		// Only the control registers (instruction pointer, flags) are read
		// and written here, so avoid the cost of a full CONTEXT_ALL fetch.
		lcContext.ContextFlags = CONTEXT_CONTROL;
		if (!GetThreadContext(hThread, &lcContext))
			return std::unexpected(MAKE_ERROR(
			    L"Error in GetThreadContext"
			    << TaleaCppCov::GetErrorMessage(GetLastError())));

		--lcContext.Rip; // Move back one byte, over the int3.
		if (setTrapFlag)
		{
			// Single-step the restored instruction so the breakpoint can be
			// re-planted on the next single-step event.
			const DWORD trapFlag = 0x100;
			lcContext.EFlags |= trapFlag;
		}

		if (!SetThreadContext(hThread, &lcContext))
			return std::unexpected(MAKE_ERROR(
			    L"Error in SetThreadContext"
			    << TaleaCppCov::GetErrorMessage(GetLastError())));
		return {};
	}

	//-------------------------------------------------------------------------
	Tools::Expected<void>
	BreakPoint::AdjustIpAfterBreakPointRemoval(HANDLE hThread) const
	{
		return RewindInstructionPointer(hThread, /*setTrapFlag*/ false);
	}

	//-------------------------------------------------------------------------
	Tools::Expected<void>
	BreakPoint::AdjustIpAndSetTrapFlag(HANDLE hThread) const
	{
		return RewindInstructionPointer(hThread, /*setTrapFlag*/ true);
	}
}
