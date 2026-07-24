// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2014 OpenCppCoverage

#pragma once

#include <iosfwd>
#include <filesystem>

#include "ExporterExport.hpp"
#include "IExporter.hpp"

namespace Coverage
{
	class CoverageData;
}

namespace Exporter
{
	class EXPORTER_DLL CoberturaExporter: public IExporter
	{
	public:
		CoberturaExporter() ;

		std::filesystem::path GetDefaultPath(const std::wstring& runningCommandFilename) const override;
		void Export(const Coverage::CoverageData&, const std::filesystem::path& output) override;
		void Export(const Coverage::CoverageData&, std::ostream&) const;

	private:
		CoberturaExporter(const CoberturaExporter&) = delete;
		CoberturaExporter& operator=(const CoberturaExporter&) = delete;
	};
}

