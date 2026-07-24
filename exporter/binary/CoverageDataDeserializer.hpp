// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2014 OpenCppCoverage

#pragma once

#include <filesystem>
#include <string>

#include "../ExporterExport.hpp"
#include "tools/Error.hpp"

namespace Coverage
{
	class CoverageData;
}

namespace Exporter
{
	class EXPORTER_DLL CoverageDataDeserializer
	{
	public:
		CoverageDataDeserializer() = default;

		// Returns the deserialized coverage, or a Tools::Error carrying
		// errorIfNotCorrectFormat when the file is not the columnar format.
		[[nodiscard]] Tools::Expected<Coverage::CoverageData>
		Deserialize(const std::filesystem::path&, const std::string& errorIfNotCorrectFormat) const;
		
	private:
		CoverageDataDeserializer(const CoverageDataDeserializer&) = delete;
		CoverageDataDeserializer& operator=(const CoverageDataDeserializer&) = delete;
	};
}

