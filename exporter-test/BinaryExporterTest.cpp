// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2014 OpenCppCoverage

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