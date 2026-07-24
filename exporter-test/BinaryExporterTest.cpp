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

#include "coverage/CoverageData.hpp"
#include "exporter/binary/BinaryExporter.hpp"
#include "exporter/InvalidOutputFileException.hpp"
#include "test-helper/TemporaryPath.hpp"
#include "tools/Tool.hpp"

namespace fs = std::filesystem;

namespace ExporterTest
{
	//-------------------------------------------------------------------------
	TEST(BinaryExporterTest, SubFolderDoesNotExist)
	{
		Coverage::CoverageData coverageData{ L"", 0 };

		TestHelper::TemporaryPath output;
		auto outputPath = output.GetPath() / "SubFolder" / "output.cov";

		ASSERT_FALSE(Tools::FileExists(outputPath));
		Exporter::BinaryExporter().Export(coverageData, outputPath);
		ASSERT_TRUE(Tools::FileExists(outputPath));
	}

	//-------------------------------------------------------------------------
	TEST(BinaryExporterTest, OutputExists)
	{
		Coverage::CoverageData coverageData{ L"", 0 };

		TestHelper::TemporaryPath outputPath{ TestHelper::TemporaryPathOption::CreateAsFile };
		
		ASSERT_NO_THROW(Exporter::BinaryExporter().Export(coverageData, outputPath.GetPath()));
	}

	//-------------------------------------------------------------------------
	TEST(BinaryExporterTest, InvalidFile)
	{
		Coverage::CoverageData coverageData{L"", 0};

		TestHelper::TemporaryPath outputPath{
		    TestHelper::TemporaryPathOption::CreateAsFolder};

		ASSERT_THROW(Exporter::BinaryExporter().Export(
		                 coverageData, outputPath.GetPath() / "InvalidFile/"),
		             Exporter::InvalidOutputFileException);
	}
}