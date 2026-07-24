// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2014 OpenCppCoverage

#include "stdafx.h"

#include "CoverageDataSerializer.hpp"
#include <fstream>

#include "coverage/CoverageData.hpp"

#include "tools/Tool.hpp"

#include "ColumnarCoverage.hpp"
#include "../InvalidOutputFileException.hpp"

namespace Exporter
{
	//-------------------------------------------------------------------------
	void CoverageDataSerializer::Serialize(
		const Coverage::CoverageData& coverageData,
		const std::filesystem::path& output) const
	{
		Tools::ThrowOnError(Tools::CreateParentFolderIfNeeded(output));

		std::ofstream ofs(output.string(), std::ios::binary);
		if (!ofs)
			throw InvalidOutputFileException(output, "binary");

		Tools::ThrowOnError(ColumnarBinary::Write(coverageData, ofs));
	}
}
