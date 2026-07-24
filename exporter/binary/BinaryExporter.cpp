// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2014 OpenCppCoverage

#include "stdafx.h"
#include "BinaryExporter.hpp"

#include <fstream>

#include <filesystem>

#include "CoverageDataSerializer.hpp"

#include "tools/Tool.hpp"

namespace Exporter
{
	//-------------------------------------------------------------------------
	std::filesystem::path BinaryExporter::GetDefaultPath(const std::wstring& prefix) const
	{
		std::filesystem::path path{ prefix };

		path += ".cov";

		return path;
	}

	//-------------------------------------------------------------------------
	void BinaryExporter::Export(
		const Coverage::CoverageData& coverageData, 
		const std::filesystem::path& output)
	{
		CoverageDataSerializer coverageDataSerializer;

		coverageDataSerializer.Serialize(coverageData, output);
		Tools::ShowOutputMessage(L"Coverage binary generated in file: ", output);
	}
}
