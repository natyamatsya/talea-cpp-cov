// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2018 OpenCppCoverage

#include "stdafx.h"
#include "FilterAssistant.hpp"
#include "IFileSystem.hpp"

#include <ctime>
#include <optional>
#include <filesystem>
#include <memory>
#include "ProgramOptions.hpp"
#include "tools/Tool.hpp"

namespace TaleaCppCov
{
	namespace
	{
		//-------------------------------------------------------------------------
		std::wstring
		GetAdviceMessage(const std::wstring& noFileSelectedMessage,
		                 const std::string& selectedOption,
		                 const std::string& excludedOption,
		                 const std::filesystem::path& suggestedFilter)
		{
			auto selectOptionFlag = L"--" + Tools::LocalToWString(selectedOption);
			auto excludedOptionFlag =
			    L"--" + Tools::LocalToWString(excludedOption);

			return noFileSelectedMessage + L" Please check the values of " +
				selectOptionFlag + L" and " + excludedOptionFlag +
				L".\n"
				L"You can try to remove all " +
				selectOptionFlag + L" and " + excludedOptionFlag +
				L" and use " + selectOptionFlag + L'=' +
				suggestedFilter.wstring() + L" instead.\n" +
				L"For additional help, see " + 
				L"https://github.com/OpenCppCoverage/OpenCppCoverage/wiki/FAQ#coverage-report-is-empty.";
		}
	}

	//-------------------------------------------------------------------------
	class FilterAssistant::SuggestedFilter
	{
	  public:
		//---------------------------------------------------------------------
		explicit SuggestedFilter(std::shared_ptr<IFileSystem> fileSystem)
		    : fileSystem_{std::move(fileSystem)}
		{
		}

		//---------------------------------------------------------------------
		void OnNewFile(const std::filesystem::path& file, bool isSelected)
		{
			if (isSelected)
			{
				foundFile_ = true;
				files_.clear();
			}

			if (!foundFile_)
				files_.push_back(file);
		}

		//-------------------------------------------------------------------------
		std::optional<std::filesystem::path> ComputeSuggestedFilter() const
		{
			if (foundFile_)
				return std::nullopt;

			std::optional<std::filesystem::path> suggestedFilter;
			std::filesystem::file_time_type newest_modification_time;
			for (const auto& file : files_)
			{
				auto time = fileSystem_->GetLastWriteTime(file);
				if (!suggestedFilter ||
				    (time && *time > newest_modification_time))
				{
					suggestedFilter = file.parent_path();
					if (time)
						newest_modification_time = *time;
				}
			}

			return suggestedFilter;
		}

		bool foundFile_ = false;

	  private:
		std::shared_ptr<IFileSystem> fileSystem_;
		std::vector<std::filesystem::path> files_;
	};

	//-------------------------------------------------------------------------
	const std::wstring FilterAssistant::NoModulesSelectedMsg =
	    L"No modules were selected.";
	const std::wstring FilterAssistant::NoSourceFilesSelectedMsg =
	    L"No source files were selected.";

	//-------------------------------------------------------------------------
	FilterAssistant::FilterAssistant(std::shared_ptr<IFileSystem> fileSystem)
	    : suggestedModuleFilter_{std::make_unique<SuggestedFilter>(fileSystem)},
	      suggestedSourceFileFilter_{std::make_unique<SuggestedFilter>(fileSystem)}
	{
	}

	//-------------------------------------------------------------------------
	FilterAssistant::~FilterAssistant() = default;

	//-------------------------------------------------------------------------
	void FilterAssistant::OnNewModule(const std::filesystem::path& module,
	                                  bool isSelected)
	{
		suggestedModuleFilter_->OnNewFile(module, isSelected);
	}

	//-------------------------------------------------------------------------
	std::optional<std::filesystem::path>
	FilterAssistant::ComputeSuggestedModuleFilter() const
	{
		return suggestedModuleFilter_->ComputeSuggestedFilter();
	}

	//-------------------------------------------------------------------------
	void
	FilterAssistant::OnNewSourceFile(const std::filesystem::path& sourceFile,
	                              bool isSelected)
	{
		suggestedSourceFileFilter_->OnNewFile(sourceFile, isSelected);
	}

	//-------------------------------------------------------------------------
	std::optional<std::filesystem::path>
	FilterAssistant::ComputeSuggestedSourceFileFilter() const
	{
		return suggestedSourceFileFilter_->ComputeSuggestedFilter();
	}

	//-------------------------------------------------------------------------
	std::optional<std::wstring> FilterAssistant::GetAdviceMessage() const
	{
		auto suggestedModule = ComputeSuggestedModuleFilter();
		if (suggestedModule)
		{
			return TaleaCppCov::GetAdviceMessage(
			    FilterAssistant::NoModulesSelectedMsg,
			    ProgramOptions::SelectedModulesOption,
			    ProgramOptions::ExcludedModulesOption,
			    *suggestedModule);
		}
		if (!suggestedModuleFilter_->foundFile_)
		{
			return std::wstring{
			    L"No modules with PDB were found. Please check your PDB "
			    L"files."};
		}

		auto suggestedSourceFile = ComputeSuggestedSourceFileFilter();
		if (suggestedSourceFile)
		{
			return TaleaCppCov::GetAdviceMessage(
			    FilterAssistant::NoSourceFilesSelectedMsg,
			    ProgramOptions::SelectedSourcesOption,
			    ProgramOptions::ExcludedSourcesOption,
			    *suggestedSourceFile);
		}

		return std::nullopt;
	}
}
