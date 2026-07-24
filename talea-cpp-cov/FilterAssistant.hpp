// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2018 OpenCppCoverage

#pragma once

#include <optional>
#include <memory>
#include <filesystem>

#include "TaleaCppCovExport.hpp"

namespace TaleaCppCov
{
	class IFileSystem;

	class TALEACPPCOV_DLL FilterAssistant
	{
	  public:
		explicit FilterAssistant(std::shared_ptr<IFileSystem>);
		~FilterAssistant();

		void OnNewModule(const std::filesystem::path&, bool isSelected);
		std::optional<std::filesystem::path>
		ComputeSuggestedModuleFilter() const;

		void OnNewSourceFile(const std::filesystem::path&, bool isSelected);
		std::optional<std::filesystem::path>
		ComputeSuggestedSourceFileFilter() const;

		std::optional<std::wstring> GetAdviceMessage() const;

		static const std::wstring NoModulesSelectedMsg;
		static const std::wstring NoSourceFilesSelectedMsg;

	  private:
		class SuggestedFilter;

		std::unique_ptr<SuggestedFilter> suggestedModuleFilter_;
		std::unique_ptr<SuggestedFilter> suggestedSourceFileFilter_;
	};
}
