// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2014 OpenCppCoverage

#include "stdafx.h"
#include "HtmlFolderStructure.hpp"

#include "tools/StringUtils.hpp"

#include "tools/Error.hpp"

#include "tools/Tool.hpp"
#include "tools/UniquePath.hpp"

namespace fs = std::filesystem;

namespace Exporter
{
	namespace
	{		
		//---------------------------------------------------------------------
		void CopyRecursiveDirectoryContent(
			const fs::path& from,
			const fs::path& to)
		{
			fs::create_directories(to);

			for (fs::recursive_directory_iterator it(from);
				it != fs::recursive_directory_iterator(); ++it)
			{
				const auto& path = it->path();
				std::string destination = path.string();
				
				Tools::ReplaceFirst(destination,
				                    std::string_view{from.string()},
				                    std::string_view{to.string()});

				if (fs::is_directory(path))
					fs::create_directories(destination);
				else
					fs::copy_file(path, destination, fs::copy_options::overwrite_existing);
			}
		}
	}

	//-------------------------------------------------------------------------
	struct HtmlFolderStructure::Hierarchy
	{
		explicit Hierarchy(const std::filesystem::path& path)
			: path_{ path } 
		{
			fs::create_directories(path);
		}

		std::filesystem::path path_;
		Tools::UniquePath uniqueChildrenPath_;
	};

	//-------------------------------------------------------------------------
	const std::wstring HtmlFolderStructure::ThirdParty = L"third-party";
	const std::wstring HtmlFolderStructure::FolderModules = L"Modules";

	//-------------------------------------------------------------------------
	HtmlFolderStructure::HtmlFolderStructure(const std::filesystem::path& templateFolder)
		: templateFolder_(templateFolder)
	{
	}

	//-------------------------------------------------------------------------
	HtmlFolderStructure::~HtmlFolderStructure() = default;

	//-------------------------------------------------------------------------
	std::filesystem::path HtmlFolderStructure::CreateCurrentRoot(const std::filesystem::path& outputFolder)
	{
		auto root{ fs::absolute(outputFolder) };
		optionalCurrentRoot_ = std::make_unique<Hierarchy>(root);
		CopyRecursiveDirectoryContent(
			templateFolder_ / HtmlFolderStructure::ThirdParty,
			root / HtmlFolderStructure::ThirdParty);

		return root;
	}

	//-------------------------------------------------------------------------
	Tools::Expected<HtmlFile> HtmlFolderStructure::CreateCurrentModule(const std::filesystem::path& modulePath)
	{
		if (!optionalCurrentRoot_)
			return std::unexpected(MAKE_ERROR(L"No root is selected"));

		auto moduleFilename = modulePath.filename();
		auto moduleName = moduleFilename.replace_extension(""); // remove extension
		auto folderModules = optionalCurrentRoot_->path_ / HtmlFolderStructure::FolderModules;

		auto moduleFolder = folderModules / moduleName;
		auto uniqueModulesFolder = optionalCurrentRoot_->uniqueChildrenPath_.GetUniquePath(moduleFolder);
		optionalCurrentModule_ = std::make_unique<Hierarchy>(uniqueModulesFolder);
		fs::path moduleHtmlPath = uniqueModulesFolder.wstring() + L".html";
		
		return HtmlFile{ 
			moduleHtmlPath, 
			fs::path{ HtmlFolderStructure::FolderModules } / moduleHtmlPath.filename() };
	}	
	
	//---------------------------------------------------------------------
	Tools::Expected<HtmlFile> HtmlFolderStructure::GetHtmlFilePath(const std::filesystem::path& filePath) const
	{
		if (!optionalCurrentModule_)
			return std::unexpected(MAKE_ERROR(L"No root module selected"));
		
		auto filename = filePath.filename();
		auto output = filename.wstring() + L".html";
		const auto& modulePath = optionalCurrentModule_->path_;
		auto fileHtmlPath = optionalCurrentModule_->uniqueChildrenPath_.GetUniquePath(modulePath / output);

		return HtmlFile{fileHtmlPath, modulePath.filename() / fileHtmlPath.filename()};
	}	
}
