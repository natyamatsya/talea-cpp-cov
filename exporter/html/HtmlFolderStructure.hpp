// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2014 OpenCppCoverage

#pragma once

#include <memory>
#include <filesystem>

#include "HtmlFile.hpp"
#include "../ExporterExport.hpp"
#include "tools/Error.hpp"

namespace Exporter
{
	class EXPORTER_DLL HtmlFolderStructure
	{
	public:
		static const std::wstring ThirdParty;
		static const std::wstring FolderModules;

	public:
		HtmlFolderStructure(const std::filesystem::path& templateFolder);
		~HtmlFolderStructure();

		std::filesystem::path CreateCurrentRoot(const std::filesystem::path& outputFolder);
		[[nodiscard]] Tools::Expected<HtmlFile> CreateCurrentModule(const std::filesystem::path&);
		[[nodiscard]] Tools::Expected<HtmlFile> GetHtmlFilePath(const std::filesystem::path& filePath) const;

	private:
		HtmlFolderStructure(const HtmlFolderStructure&) = delete;
		HtmlFolderStructure& operator=(const HtmlFolderStructure&) = delete;

	private:
		std::filesystem::path templateFolder_;

		struct Hierarchy;
		std::unique_ptr<Hierarchy> optionalCurrentRoot_;
		std::unique_ptr<Hierarchy> optionalCurrentModule_;
	};
}

