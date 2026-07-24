// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2014 OpenCppCoverage

#pragma once

#include <filesystem>
#include "../ExporterExport.hpp"

namespace Coverage
{
	class CoverageData;
}

namespace Exporter
{
	class EXPORTER_DLL CoverageDataSerializer
	{
	public:
		CoverageDataSerializer() = default;

		void Serialize(const Coverage::CoverageData&, const std::filesystem::path&) const;

	private:
		CoverageDataSerializer(const CoverageDataSerializer&) = delete;
		CoverageDataSerializer& operator=(const CoverageDataSerializer&) = delete;
	};
}

