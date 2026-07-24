// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 natyamatsya and talea-cpp-cov contributors

#pragma once

#include <filesystem>
#include <memory>

#include "TaleaCppCovExport.hpp"
#include "coverage/CoverageData.hpp"
#include "tools/Error.hpp"

namespace Tools
{
	class WarningManager;
}

namespace TaleaCppCov
{
	class RunCoverageSettings;
	class PdbLineTableCache;
	class FilterAssistant;

	// Experimental in-process coverage engine (roadmap Tier 3.2).
	//
	// Instead of debugging the target, it launches it suspended, injects a
	// tiny helper DLL, and lets a Vectored Exception Handler service the int3
	// probes inside the process - a usermode exception dispatch per first hit
	// instead of the cross-process debug-event round trip the debugger engine
	// pays. Probe placement (source/line selection, straight-line run merging)
	// is computed entirely offline from the PDB and the module file, so it
	// reuses the same machinery as the debugger engine and produces the same
	// coverage data.
	//
	// Covers the main executable, filter-matching DLLs, branch coverage, and
	// child processes (recursively, via an in-process CreateProcess IAT hook).
	// 64-bit only; the caller uses the debugger engine for 32-bit targets.
	class TALEACPPCOV_DLL VehCoverageEngine
	{
	  public:
		explicit VehCoverageEngine(std::shared_ptr<Tools::WarningManager>);
		~VehCoverageEngine();

		[[nodiscard]] Tools::Expected<Coverage::CoverageData>
		RunCoverage(const RunCoverageSettings&);

		// Absolute path of the helper DLL, next to the running module.
		static std::filesystem::path GetHelperDllPath();

	  private:
		VehCoverageEngine(const VehCoverageEngine&) = delete;
		VehCoverageEngine& operator=(const VehCoverageEngine&) = delete;

		std::shared_ptr<Tools::WarningManager> warningManager_;
		std::shared_ptr<FilterAssistant> filterAssistant_;
	};
}
