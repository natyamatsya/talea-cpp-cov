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

#include <filesystem>
#include <fstream>
#include <regex>


#include "Plugin/Exporter/CoverageData.hpp"
#include "Plugin/Exporter/ModuleCoverage.hpp"
#include "Plugin/Exporter/FileCoverage.hpp"

#include "Exporter/InvalidOutputFileException.hpp"
#include "Exporter/CoberturaExporter.hpp"
#include "Tools/Tool.hpp"

#include "TestHelper/TemporaryPath.hpp"

namespace fs = std::filesystem;

namespace ExporterTest
{
	namespace
	{
		//-------------------------------------------------------------------------
		// The report is UTF-8 bytes; read the golden file as narrow bytes too.
		// Text-mode ifstream normalizes the golden file's CRLF to '\n', matching
		// the '\n' the exporter writes into its in-memory buffer.
		std::string GetExpectedResult(const std::string& filename)
		{
			fs::path expectedResult = fs::path(PROJECT_DIR) / "Data" / filename;
			std::ifstream ifs{ expectedResult.wstring().c_str() };
			std::ostringstream ostr;

			ostr << ifs.rdbuf();

			return ostr.str();
		}

		//-------------------------------------------------------------------------
		std::string GetExpectedResult()
		{
			return GetExpectedResult("CoberturaExporterExpectedResult.xml");
		}

		//-------------------------------------------------------------------------
		std::string ExportToString(const Plugin::CoverageData& coverageData)
		{
			std::ostringstream ostr;
			Exporter::CoberturaExporter().Export(coverageData, ostr);
			auto result = ostr.str();
			std::regex regex(R"(timestamp="\d*")");
			return std::regex_replace(result, regex, "timestamp=\"TIMESTAMP\"");
		}
	}
	
	//-------------------------------------------------------------------------
	TEST(CoberturaExporterTest, Export)
	{
		Plugin::CoverageData coverageData{L"", 0};

		coverageData.AddModule(L"EmptyModule");
		auto& module = coverageData.AddModule(L"Module");

		module.AddFile("EmptyFile");
		auto& file = module.AddFile("File");

		file.AddLine(0, true);
		file.AddLine(1, false);

		module.AddFile("File2").AddLine(0, true);

		std::ostringstream ostr;
		Exporter::CoberturaExporter().Export(coverageData, ostr);
		auto result = ostr.str();
		std::regex regex(R"(timestamp="\d*")");
		result = std::regex_replace(result, regex, "timestamp=\"TIMESTAMP\"");

		auto expectedResult = GetExpectedResult();

		ASSERT_EQ(result, expectedResult);
	}

	//-------------------------------------------------------------------------
	TEST(CoberturaExporterTest, ExportWithBranches)
	{
		Plugin::CoverageData coverageData{L"", 0};

		auto& module = coverageData.AddModule(L"Module");
		auto& file = module.AddFile("File");

		file.AddLine(10, true);
		file.AddBranch(10, Plugin::BranchCoverage{0, true, false});
		file.AddBranch(10, Plugin::BranchCoverage{1, true, true});

		file.AddLine(20, false);
		file.AddBranch(20, Plugin::BranchCoverage{0, false, false});

		file.AddLine(30, true);
		file.AddLine(40, false);

		auto result = ExportToString(coverageData);
		auto expectedResult =
		    GetExpectedResult("CoberturaExporterBranchExpectedResult.xml");

		ASSERT_EQ(result, expectedResult);
	}

	//-------------------------------------------------------------------------
	TEST(CoberturaExporterTest, SubFolderDoesNotExist)
	{
		Plugin::CoverageData coverageData{ L"", 0 };
		TestHelper::TemporaryPath output;
		auto outputPath = output.GetPath() / "SubFolder" / "output.xml";

		ASSERT_FALSE(Tools::FileExists(outputPath));
		Exporter::CoberturaExporter().Export(coverageData, outputPath);
		ASSERT_TRUE(Tools::FileExists(outputPath));
	}

	//-------------------------------------------------------------------------
	TEST(CoberturaExporterTest, SpecialChars)
	{
		Plugin::CoverageData coverageData{ L"", 0 };
		coverageData.AddModule(L"\u00E9\u00E0").AddFile(L"\u00E9\u00E0").AddLine(0, true);

		std::ostringstream ostr;
		Exporter::CoberturaExporter().Export(coverageData, ostr);
		auto result = ostr.str();

		// The report is UTF-8, so the accented names appear as their UTF-8
		// bytes directly - no widening needed.
		auto packageName = reinterpret_cast<const char*>(u8"package name=\"\u00E9\u00E0\"");
		auto name = reinterpret_cast<const char*>(u8"class name=\"\u00E9\u00E0\"");
		auto filename = reinterpret_cast<const char*>(u8"filename=\"\u00E9\u00E0\"");

		ASSERT_TRUE(result.contains(packageName));
		ASSERT_TRUE(result.contains(name));
		ASSERT_TRUE(result.contains(filename));
	}

	//-------------------------------------------------------------------------
	TEST(CoberturaExporterTest, OutputExists)
	{
		Plugin::CoverageData coverageData{ L"", 0 };
		TestHelper::TemporaryPath outputPath{ TestHelper::TemporaryPathOption::CreateAsFile };
		
		ASSERT_NO_THROW(Exporter::CoberturaExporter().Export(coverageData, outputPath));
	}

	//-------------------------------------------------------------------------
	TEST(CoberturaExporterTest, InvalidFile)
	{
		Plugin::CoverageData coverageData{L"", 0};
		TestHelper::TemporaryPath outputPath{
		    TestHelper::TemporaryPathOption::CreateAsFolder};

		ASSERT_THROW(Exporter::CoberturaExporter().Export(
		                 coverageData, outputPath.GetPath() / "InvalidFile/"),
		             Exporter::InvalidOutputFileException);
	}
}