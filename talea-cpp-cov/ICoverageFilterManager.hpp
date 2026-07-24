// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2016 OpenCppCoverage

#pragma once

#include "TaleaCppCovExport.hpp"
#include <string>

namespace FileFilter
{
	class ModuleInfo;
	class FileInfo;
	class LineInfo;
}

namespace TaleaCppCov
{
	class CoverageFilterSettings;

	class TALEACPPCOV_DLL ICoverageFilterManager
	{
	public:
		virtual ~ICoverageFilterManager() = default;

		virtual bool IsModuleSelected(const std::wstring& filename) const = 0;
		virtual bool IsSourceFileSelected(const std::wstring& filename) = 0;
		virtual bool IsLineSelected(
			const FileFilter::ModuleInfo&,
			const FileFilter::FileInfo&,
			const FileFilter::LineInfo&) = 0;
	};
}


