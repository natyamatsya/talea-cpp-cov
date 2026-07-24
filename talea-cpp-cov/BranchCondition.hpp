// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2014 OpenCppCoverage
// SPDX-FileCopyrightText: 2026 natyamatsya and talea-cpp-cov contributors

#pragma once

#include <Windows.h>
#include "TaleaCppCovExport.hpp"
#include "BranchInstruction.hpp"

namespace TaleaCppCov
{
	// Evaluates whether a conditional jump is taken given the thread's
	// EFLAGS register (and RCX for JCXZ/JECXZ/JRCXZ).
	bool TALEACPPCOV_DLL IsBranchTaken(BranchCondition condition,
	                                   DWORD eflags,
	                                   DWORD64 rcx);
}
