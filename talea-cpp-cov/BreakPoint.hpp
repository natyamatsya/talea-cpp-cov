// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2014 OpenCppCoverage

#pragma once
#include <cstdint>

#include <Windows.h>
#include "TaleaCppCovExport.hpp"
#include "tools/Error.hpp"

namespace TaleaCppCov
{
	class Address;

	class TALEACPPCOV_DLL BreakPoint
	{
	  public:
		BreakPoint() = default;

		static const std::uint8_t breakPointInstruction;

		[[nodiscard]] Tools::Expected<void>
		RemoveBreakPoint(const Address&, std::uint8_t oldInstruction) const;

		using InstructionCollection =
		    std::vector<std::pair<std::uint8_t, DWORD64>>;

		[[nodiscard]] Tools::Expected<InstructionCollection>
		SetBreakPoints(HANDLE hProcess, std::vector<DWORD64>&& addresses) const;

		[[nodiscard]] Tools::Expected<void> SetBreakPoint(const Address&) const;

		[[nodiscard]] Tools::Expected<void>
		AdjustIpAfterBreakPointRemoval(HANDLE hThread) const;

		[[nodiscard]] Tools::Expected<void>
		AdjustIpAndSetTrapFlag(HANDLE hThread) const;

	  private:
		BreakPoint(const BreakPoint&) = delete;
		BreakPoint& operator=(const BreakPoint&) = delete;

		// Rewinds the thread's instruction pointer by one byte (past the int3),
		// optionally arming the trap flag to single-step the restored
		// instruction. Only the control registers are touched.
		[[nodiscard]] Tools::Expected<void>
		RewindInstructionPointer(HANDLE hThread, bool setTrapFlag) const;
	};
}
