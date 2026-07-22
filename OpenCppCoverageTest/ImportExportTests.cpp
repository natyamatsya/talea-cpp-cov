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

#include "TestCoverageConsole/TestCoverageConsole.hpp"
#include "TestHelper/TemporaryPath.hpp"

#include "CppCoverage/ExportOptionParser.hpp"
#include "CppCoverage/ProgramOptions.hpp"
#include "Plugin/Exporter/CoverageData.hpp"
#include "Plugin/Exporter/ModuleCoverage.hpp"
#include "Plugin/Exporter/FileCoverage.hpp"
#include "Plugin/Exporter/LineCoverage.hpp"
#include "CppCoverage/OptionsParser.hpp"

#include "Exporter/Binary/CoverageDataDeserializer.hpp"

#include "TestHelper/CoverageDataComparer.hpp"
#include "TestCoverageSharedLib/TestCoverageSharedLib.hpp"

#include "Tools/Tool.hpp"

#include "OpenCppCoverageTestTools.hpp"

namespace fs = std::filesystem;
namespace cov = CppCoverage;

namespace OpenCppCoverageTest
{
	namespace
	{		
		
		//---------------------------------------------------------------------
		void RunCoverage(
			std::vector<std::pair<std::string, std::string>> coverageArguments,
			const std::filesystem::path& output)
		{
			fs::path testCoverageConsole = TestCoverageConsole::GetOutputBinaryPath();						
			
			AddDefaultFilters(coverageArguments, testCoverageConsole);
			coverageArguments.emplace_back(cov::ProgramOptions::QuietOption, "");
			int exitCode = RunCoverageFor(coverageArguments, testCoverageConsole, {});
			
			ASSERT_EQ(0, exitCode);		
			ASSERT_TRUE(Tools::FileExists(output));
		}
		
		//---------------------------------------------------------------------
		void RunCoverage(const std::string& exportType)
		{
			TestHelper::TemporaryPath output;
			std::vector<std::pair<std::string, std::string>> coverageArguments;

			coverageArguments.push_back(BuildExportTypeString(exportType, output));
			RunCoverage(coverageArguments, output);
		}

		//---------------------------------------------------------------------
		Plugin::CoverageData ReadCoverageDataFromFile(const TestHelper::TemporaryPath& temporaryPath)
		{
			Exporter::CoverageDataDeserializer coverageDataDeserializer;
			
			return coverageDataDeserializer.Deserialize(temporaryPath.GetPath(), "");
		}
	}

	//-------------------------------------------------------------------------
	TEST(ImportExportTest, ExportHtml)
	{
		RunCoverage(cov::ExportOptionParser::ExportTypeHtmlValue);
	}

	//-------------------------------------------------------------------------
	TEST(ImportExportTest, ExportCobertura)
	{
		RunCoverage(cov::ExportOptionParser::ExportTypeCoberturaValue);
	}

	//-------------------------------------------------------------------------
	TEST(ImportExportTest, ExportImportBinary)
	{
		TestHelper::TemporaryPath initialOutput;
		RunCoverage({ BuildExportTypeString(cov::ExportOptionParser::ExportTypeBinaryValue, initialOutput) }, initialOutput);

		TestHelper::TemporaryPath finalOutput;

		RunCoverage(
		{ { cov::ProgramOptions::InputCoverageValue, initialOutput.GetPath().string() },
		{ BuildExportTypeString(cov::ExportOptionParser::ExportTypeBinaryValue, finalOutput )} },
		finalOutput);	

		auto initialCoverage = ReadCoverageDataFromFile(initialOutput);
		auto finalCoverage = ReadCoverageDataFromFile(finalOutput);

		TestHelper::CoverageDataComparer coverageDataComparer;

		coverageDataComparer.AssertEquals(initialCoverage, finalCoverage);
	}	

	//-------------------------------------------------------------------------
	TEST(ImportExportTest, ExportImportBinaryWithBranches)
	{
		fs::path testCoverageConsole =
		    TestCoverageConsole::GetOutputBinaryPath();
		const std::vector<std::wstring> programArguments{
		    TestCoverageConsole::TestBranch, L"1"};
		TestHelper::TemporaryPath initialOutput;

		std::vector<std::pair<std::string, std::string>> coverageArguments{
		    BuildExportTypeString(
		        cov::ExportOptionParser::ExportTypeBinaryValue, initialOutput),
		    {cov::ProgramOptions::BranchCoverageOption, ""}};
		AddDefaultFilters(coverageArguments, testCoverageConsole);
		coverageArguments.emplace_back(cov::ProgramOptions::QuietOption, "");
		ASSERT_EQ(0,
		          RunCoverageFor(
		              coverageArguments, testCoverageConsole, programArguments));

		auto initialCoverage = ReadCoverageDataFromFile(initialOutput);

		// The branch data must survive the import of the binary export.
		// Run the same program again to have the same coverage.
		TestHelper::TemporaryPath finalOutput;
		std::vector<std::pair<std::string, std::string>> importArguments{
		    {cov::ProgramOptions::InputCoverageValue,
		     initialOutput.GetPath().string()},
		    BuildExportTypeString(
		        cov::ExportOptionParser::ExportTypeBinaryValue, finalOutput),
		    {cov::ProgramOptions::BranchCoverageOption, ""}};
		AddDefaultFilters(importArguments, testCoverageConsole);
		importArguments.emplace_back(cov::ProgramOptions::QuietOption, "");
		ASSERT_EQ(0,
		          RunCoverageFor(
		              importArguments, testCoverageConsole, programArguments));
		auto finalCoverage = ReadCoverageDataFromFile(finalOutput);

		TestHelper::CoverageDataComparer().AssertEquals(initialCoverage,
		                                                finalCoverage);

		// The initial coverage data must contain branches.
		bool hasBranches = false;
		for (const auto& module : initialCoverage.GetModules())
		{
			for (const auto& file : module->GetFiles())
			{
				for (const auto& line : file->GetLines())
				{
					if (!line.GetBranches().empty())
						hasBranches = true;
				}
			}
		}
		ASSERT_TRUE(hasBranches);
	}

	//-------------------------------------------------------------------------
	TEST(ImportExportTest, ExportPlugin)
	{
		TestHelper::TemporaryPath tempPath{
		    TestHelper::TemporaryPathOption::CreateAsFolder};
		auto output = tempPath.GetPath() / "Output.txt";
		auto dllPath = TestCoverageSharedLib::GetOutputBinaryPath();
		auto pluginName = dllPath.stem().string();

		RunCoverage({BuildExportTypeString(pluginName, output)}, output);
		ASSERT_NE(0, std::filesystem::file_size(output));
	}
}
