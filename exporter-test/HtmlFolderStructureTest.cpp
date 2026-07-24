// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2014 OpenCppCoverage

#include "stdafx.h"

#include <system_error>

#include "exporter/html/HtmlFolderStructure.hpp"

#include "test-helper/TemporaryPath.hpp"
#include "test-helper/Tools.hpp"

#include "tools/Tool.hpp"

namespace fs = std::filesystem;

namespace ExporterTest
{
	namespace
	{
		//-------------------------------------------------------------------------
		struct HtmlFolderStructureTest : public ::testing::Test
		{
			HtmlFolderStructureTest()
			{
				std::error_code error;
				auto folder = templateFolder_.GetPath() / Exporter::HtmlFolderStructure::ThirdParty;
				fs::create_directories(folder);				
				htmlFolderStructure_.reset(new Exporter::HtmlFolderStructure(templateFolder_));				
			}

			std::unique_ptr<Exporter::HtmlFolderStructure> htmlFolderStructure_;

			TestHelper::TemporaryPath templateFolder_;
			TestHelper::TemporaryPath outputFolder_;
		};
		
		//-------------------------------------------------------------------------
		const std::string Module = "Module";
		const std::string File = "file";
	}

	//-------------------------------------------------------------------------
	TEST_F(HtmlFolderStructureTest, CreateCurrentRoot)
	{						
		TestHelper::CreateEmptyFile(templateFolder_.GetPath() / Exporter::HtmlFolderStructure::ThirdParty / "EmptyFile");
		htmlFolderStructure_->CreateCurrentRoot(outputFolder_);
		auto createdPath = outputFolder_.GetPath() / Exporter::HtmlFolderStructure::ThirdParty;
		ASSERT_TRUE(Tools::FileExists(createdPath));	
	}
	
	//-------------------------------------------------------------------------
	TEST_F(HtmlFolderStructureTest, CreateCurrentModule)
	{		
		htmlFolderStructure_->CreateCurrentRoot(outputFolder_);
		auto htmlPath = Tools::ValueOrThrow(
		    htmlFolderStructure_->CreateCurrentModule(Module + ".exe"));
		auto modulesFolder = outputFolder_.GetPath() / Exporter::HtmlFolderStructure::FolderModules;
		auto expectedOutputFolder = modulesFolder / Module;
		ASSERT_TRUE(Tools::FileExists(expectedOutputFolder));
		auto expectedPath = modulesFolder / (Module + ".html");
		ASSERT_EQ(expectedPath, htmlPath.GetAbsolutePath());
	}

	//-------------------------------------------------------------------------
	TEST_F(HtmlFolderStructureTest, GetHtmlFilePath)
	{		
		htmlFolderStructure_->CreateCurrentRoot(outputFolder_);
		Tools::ValueOrThrow(htmlFolderStructure_->CreateCurrentModule(Module));
		auto htmlFilePath =
		    Tools::ValueOrThrow(htmlFolderStructure_->GetHtmlFilePath(File));

		auto expectedPath = outputFolder_.GetPath();
		
		expectedPath /= Exporter::HtmlFolderStructure::FolderModules;		
		expectedPath /= Module;
		expectedPath /= File + ".html";
		ASSERT_EQ(expectedPath, htmlFilePath.GetAbsolutePath());
	}

	//-------------------------------------------------------------------------
	TEST_F(HtmlFolderStructureTest, TestConflictModules)
	{
		htmlFolderStructure_->CreateCurrentRoot(outputFolder_);		

		auto module =
		    Tools::ValueOrThrow(htmlFolderStructure_->CreateCurrentModule(Module));
		auto modulePath = module.GetAbsolutePath();

		TestHelper::CreateEmptyFile(modulePath);
		auto otherModule =
		    Tools::ValueOrThrow(htmlFolderStructure_->CreateCurrentModule(Module));

		ASSERT_NE(modulePath, otherModule.GetAbsolutePath());
	} 

	//-------------------------------------------------------------------------
	TEST_F(HtmlFolderStructureTest, TestConflictFile)
	{
		htmlFolderStructure_->CreateCurrentRoot(outputFolder_);
		Tools::ValueOrThrow(htmlFolderStructure_->CreateCurrentModule(Module));

		auto file =
		    Tools::ValueOrThrow(htmlFolderStructure_->GetHtmlFilePath(File));
		auto filePath = file.GetAbsolutePath();

		TestHelper::CreateEmptyFile(filePath);
		auto otherFile =
		    Tools::ValueOrThrow(htmlFolderStructure_->GetHtmlFilePath(File));

		ASSERT_NE(filePath, otherFile.GetAbsolutePath());
	}
}