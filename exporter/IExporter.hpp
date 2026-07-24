// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2014 OpenCppCoverage

#pragma once

#include <string>
#include <filesystem>

namespace Coverage
{
	class CoverageData;
}

namespace Exporter
{
	class EXPORTER_DLL IExporter
	{
	public:
		IExporter() = default;
		virtual ~IExporter() = default;

		virtual std::filesystem::path GetDefaultPath(const std::wstring& prefix) const = 0;
		virtual void Export(const Coverage::CoverageData&, const std::filesystem::path& output) = 0;

	private:
		IExporter(const IExporter&) = delete;
		IExporter& operator=(const IExporter&) = delete;
	};
}

