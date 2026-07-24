// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2014 OpenCppCoverage

#include "stdafx.h"

#include "coverage/CoverageData.hpp"
#include "coverage/ModuleCoverage.hpp"
#include "coverage/FileCoverage.hpp"
#include "test-helper/TemporaryPath.hpp"

#include <fstream>
#include <filesystem>

#include "exporter/html/HtmlExporter.hpp"
#include "exporter/html/HtmlFolderStructure.hpp"

#include "test-helper/TemporaryPath.hpp"

#include "tools/Tool.hpp"

namespace fs = std::filesystem;

namespace ExporterTest
{
	//-------------------------------------------------------------------------
	struct HtmlExporterTest : public ::testing::Test
	{
		//-------------------------------------------------------------------------
		HtmlExporterTest()
			: htmlExporter_{ fs::canonical(OUT_DIR) / "Template" }
		{

		}

		//---------------------------------------------------------------------
		bool Contains(std::wistream& istr, const std::wstring& str)
		{			
			std::wstring line;

			while (std::getline(istr, line))
			{
				if (line.find(str) != std::string::npos)
					return true;
			}

			return false;
		}

		//---------------------------------------------------------------------
		void CheckWarningInIndex(bool expectedValue)
		{
			auto indexPath = output_.GetPath() / "index.html";
			ASSERT_TRUE(Tools::FileExists(indexPath));
			std::wifstream ifs{ indexPath.string()};
			bool hasWarning = Contains(ifs, Exporter::HtmlExporter::WarningExitCodeMessage);
			
			ASSERT_EQ(expectedValue, hasWarning);
		}

		Exporter::HtmlExporter htmlExporter_;
		TestHelper::TemporaryPath output_;
	};	

	//-------------------------------------------------------------------------
	TEST_F(HtmlExporterTest, Export)
	{	
		fs::path testFolder = fs::path(PROJECT_DIR) / "data";
		Coverage::CoverageData data{ L"Test", 0};
		std::wstring filename1{ L"TestFile1.cpp"};
		std::wstring filename2{ L"TestFile2.cpp"};

		auto& module1 = data.AddModule(L"Module1.exe");
		auto& file1 = module1.AddFile(testFolder / filename1);
		auto& file2 = module1.AddFile(testFolder / filename2);
		
		file1.AddLine(0, true);
		file2.AddLine(0, true);
		
		data.AddModule(L"Module2.exe");		

		htmlExporter_.Export(data, output_);

		auto modulesPath = output_.GetPath() / Exporter::HtmlFolderStructure::FolderModules;
		ASSERT_TRUE(Tools::FileExists(output_.GetPath() / "index.html"));
		ASSERT_TRUE(Tools::FileExists(modulesPath / "module1.html"));
		ASSERT_FALSE(Tools::FileExists(modulesPath / "module2.html"));
		ASSERT_TRUE(Tools::FileExists(modulesPath / "module1" / (filename1 + L".html")));
		ASSERT_TRUE(Tools::FileExists(modulesPath / "module1" / (filename2 + L".html")));
	}

	//-------------------------------------------------------------------------
	TEST_F(HtmlExporterTest, NoWarning)
	{
		Coverage::CoverageData data{ L"Test", 0 };

		htmlExporter_.Export(data, output_);		
		CheckWarningInIndex(false);
	}

	//-------------------------------------------------------------------------
	TEST_F(HtmlExporterTest, Warning)
	{
		Coverage::CoverageData data{ L"Test", 42};

		htmlExporter_.Export(data, output_);	
		CheckWarningInIndex(true);
	}

	//-------------------------------------------------------------------------
	TEST_F(HtmlExporterTest, SubFolderDoesNotExist)
	{
		Coverage::CoverageData data{ L"Test", 42 };
		auto outputFolder = output_.GetPath() / "SubFolder1" / "SubFolder2";

		ASSERT_FALSE(Tools::FileExists(outputFolder));
		htmlExporter_.Export(data, outputFolder);
		ASSERT_TRUE(Tools::FileExists(outputFolder));
	}

	//-------------------------------------------------------------------------
	TEST_F(HtmlExporterTest, OutputExists)
	{
		Coverage::CoverageData data{ L"Test", 42 };
		TestHelper::TemporaryPath outputFolder{ TestHelper::TemporaryPathOption::CreateAsFolder };
		
		ASSERT_NO_THROW(htmlExporter_.Export(data, outputFolder));
	}

	//-------------------------------------------------------------------------
	TEST_F(HtmlExporterTest, SameModuleSameSourceFile)
	{
		Coverage::CoverageData data{ L"Test", 0 };
		const std::wstring filename = L"TestFile1.cpp";
		const auto moduleName = L"Module.exe";
		
		auto& module = data.AddModule(moduleName);
		module.AddFile(fs::path(PROJECT_DIR) / "data" / filename).AddLine(0, true);
		module.AddFile(fs::path(PROJECT_DIR) / "data" / filename);
		data.AddModule(moduleName).AddFile("file").AddLine(0, true);
		
		htmlExporter_.Export(data, output_);

		auto modulesPath = output_.GetPath() / Exporter::HtmlFolderStructure::FolderModules;
		ASSERT_TRUE(Tools::FileExists(modulesPath / "module2"));
		ASSERT_TRUE(Tools::FileExists(modulesPath / "module2.html"));
		ASSERT_TRUE(Tools::FileExists(modulesPath / "module" / (filename + L".html")));
		ASSERT_TRUE(Tools::FileExists(modulesPath / "module" / (filename + L"2.html")));
		ASSERT_TRUE(Tools::FileExists(modulesPath / "module.html"));
	}
}

