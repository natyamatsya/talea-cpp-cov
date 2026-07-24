// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2014 OpenCppCoverage

#pragma once
#include <cstdint>

#include <iosfwd>

#include "../ExporterExport.hpp"
#include "tools/Error.hpp"

namespace Coverage
{
	class FileCoverage;
}

namespace Exporter
{
	class EXPORTER_DLL HtmlFileCoverageExporter
	{
	public:
		static const std::wstring StyleBackgroundColorExecuted;
		static const std::wstring StyleBackgroundColorUnexecuted;
		static const std::wstring EndStyle;

	public:
		HtmlFileCoverageExporter(
			std::int32_t maxSourceLineCount = 8000, 
			std::int32_t maxSourceLineStyleChangesCount = 1000,
			std::int32_t maxStyleChangesCount = 2000);

		[[nodiscard]] Tools::Expected<bool> Export(
			const Coverage::FileCoverage&,
			std::wostream& output) const;
		
		bool MustEnableCodePrettify(std::int32_t lineCount, std::int32_t styleChangedCount) const;

	private:
		HtmlFileCoverageExporter(const HtmlFileCoverageExporter&) = delete;
		HtmlFileCoverageExporter& operator=(const HtmlFileCoverageExporter&) = delete;

		std::int32_t maxSourceLineCount_;
		std::int32_t maxSourceLineStyleChangesCount_;
		std::int32_t maxStyleChangesCount_;
	};
}


