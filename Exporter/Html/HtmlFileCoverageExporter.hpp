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

#pragma once
#include <cstdint>

#include <iosfwd> 

#include "../ExporterExport.hpp"

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

		bool Export(
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


