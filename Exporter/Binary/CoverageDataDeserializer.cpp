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

#include "CoverageDataDeserializer.hpp"

#include "Coverage/CoverageData.hpp"

#include "ColumnarCoverage.hpp"
#include "Tools/Tool.hpp"

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
