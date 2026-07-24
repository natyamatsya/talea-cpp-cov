// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2017 OpenCppCoverage

#pragma once

#include <vector>
#include "StartInfo.hpp"
#include "UnifiedDiffSettings.hpp"
#include "CoverageFilterSettings.hpp"

#include "TaleaCppCovExport.hpp"
#include "SubstitutePdbSourcePath.hpp"

namespace TaleaCppCov
{
	class TALEACPPCOV_DLL RunCoverageSettings
	{
	public:
		RunCoverageSettings(
			const StartInfo&,
			const CoverageFilterSettings&,
			const std::vector<UnifiedDiffSettings>&,
			const std::vector<std::wstring>& excludedLineRegexes,
			const std::vector<SubstitutePdbSourcePath>&);

		RunCoverageSettings(const RunCoverageSettings&) = delete;
		RunCoverageSettings& operator=(const RunCoverageSettings&) = delete;

		void SetCoverChildren(bool);
		void SetContinueAfterCppException(bool);
        void SetStopOnAssert(bool);
        void SetMaxUnmatchPathsForWarning(size_t);
		void SetOptimizedBuildSupport(bool);
		void SetBranchCoverage(bool);
		void SetPdbCacheEnabled(bool);
		void SetRawPdbEnabled(bool);
		void SetLineMergingEnabled(bool);
		void SetPreparseImports(bool);

		const StartInfo& GetStartInfo() const;
		const CoverageFilterSettings& GetCoverageFilterSettings() const;
		const std::vector<UnifiedDiffSettings>& GetUnifiedDiffSettings() const;
		bool GetCoverChildren() const;
		bool GetContinueAfterCppException() const;
        bool GetStopOnAssert() const;
        size_t GetMaxUnmatchPathsForWarning() const;
		bool GetOptimizedBuildSupport() const;
		bool GetBranchCoverage() const;
		bool IsPdbCacheEnabled() const;
		bool IsRawPdbEnabled() const;
		bool IsLineMergingEnabled() const;
		bool GetPreparseImports() const;
		const std::vector<std::wstring>& GetExcludedLineRegexes() const;
		const std::vector<SubstitutePdbSourcePath>& GetSubstitutePdbSourcePaths() const;

	private:
		StartInfo startInfo_;
		CoverageFilterSettings coverageFilterSettings_;
		std::vector<UnifiedDiffSettings> unifiedDiffSettings_;
		bool coverChildren_;
		bool continueAfterCppException_;
        bool stopOnAssert_;
        size_t maxUnmatchPathsForWarning_;
		bool optimizedBuildSupport_;
		bool branchCoverage_;
		bool pdbCacheEnabled_;
		bool rawPdbEnabled_;
		bool lineMergingEnabled_;
		bool preparseImports_;
		std::vector<std::wstring> excludedLineRegexes_;
		std::vector<SubstitutePdbSourcePath> substitutePdbSourcePath_;
	};
}
