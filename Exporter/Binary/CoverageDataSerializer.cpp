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

#include "stdafx.h"

// Protobuf and abseil headers raise conversion warnings when their
// templates are instantiated, which happens after any warning(pop).
#pragma warning(disable : 4018 4244 4267)

#include "CoverageDataSerializer.hpp"
#include <fstream>

#include "Plugin/Exporter/CoverageData.hpp"

#include "Tools/Tool.hpp"

#include "ColumnarCoverage.hpp"
#include "../InvalidOutputFileException.hpp"

namespace Exporter
{
	//-------------------------------------------------------------------------
	// Kept for the legacy v1 (protobuf) magic the deserializer still accepts;
	// new files are written in the columnar v2 format below.
	const unsigned int CoverageDataSerializer::FileTypeId = 1351727964; // random number

	//-------------------------------------------------------------------------
	void CoverageDataSerializer::Serialize(
		const Plugin::CoverageData& coverageData,
		const std::filesystem::path& output) const
	{
		Tools::CreateParentFolderIfNeeded(output);

		std::ofstream ofs(output.string(), std::ios::binary);
		if (!ofs)
			throw InvalidOutputFileException(output, "binary");

		ColumnarBinary::Write(coverageData, ofs);
	}
}
