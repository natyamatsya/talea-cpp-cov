// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2014 OpenCppCoverage

#pragma once

#include "../ExporterExport.hpp"
#include "../IExporter.hpp"

namespace Exporter
{
	class EXPORTER_DLL BinaryExporter : public IExporter
	{
	public:
		BinaryExporter() = default;

		std::filesystem::path GetDefaultPath(const std::wstring& prefix) const override;
		void Export(const Coverage::CoverageData&, const std::filesystem::path& output) override;

	private:
		BinaryExporter(const BinaryExporter&) = delete;
		BinaryExporter& operator=(const BinaryExporter&) = delete;
	};
}

