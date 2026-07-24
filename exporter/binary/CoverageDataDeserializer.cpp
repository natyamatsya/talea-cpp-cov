// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2014 OpenCppCoverage

#include "stdafx.h"

#include "CoverageDataDeserializer.hpp"

#include "coverage/CoverageData.hpp"

#include "ColumnarCoverage.hpp"
#include "tools/Tool.hpp"

namespace Exporter
{
	//-------------------------------------------------------------------------
	Tools::Expected<Coverage::CoverageData> CoverageDataDeserializer::Deserialize(
		const std::filesystem::path& path,
		const std::string& errorIfNotCorrectFormat) const
	{
		// The only supported format is the columnar v2 file the serializer
		// writes (identified by its magic). The original protobuf ".cov" format
		// is no longer read.
		if (!ColumnarBinary::IsColumnarFile(path))
			return std::unexpected(
			    Tools::Error{Tools::LocalToWString(errorIfNotCorrectFormat)});

		return ColumnarBinary::Read(path);
	}
}
