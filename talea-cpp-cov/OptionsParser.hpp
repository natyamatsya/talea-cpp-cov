// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2014 OpenCppCoverage

#pragma once
#include <cstdint>

#include <optional>
#include <iosfwd>
#include <memory>
#include <string>
#include <vector>

#include "TaleaCppCovExport.hpp"
#include "tools/Error.hpp"

namespace Tools
{
	class WarningManager;
}

namespace TaleaCppCov
{
	class Options;
	enum class OptionsExportType;
	class IOptionParser;

	class TALEACPPCOV_DLL OptionsParser
	{
	public:
		static const char PathSeparator;
		static const std::int32_t DosCommandLineMaxSize;

		OptionsParser();
		explicit OptionsParser(std::shared_ptr<Tools::WarningManager>,
		                       std::vector<std::unique_ptr<IOptionParser>>&&);
		~OptionsParser();

		std::optional<Options> Parse(int argc, const char** argv, std::wostream* emptyOptionsExplanation) const;
				
		static std::wstring GetTooLongCommandLineMessage();

	private:
		OptionsParser(const OptionsParser&) = delete;
		OptionsParser& operator=(const OptionsParser&) = delete;
		OptionsParser(OptionsParser&&) = delete;
		OptionsParser& operator=(OptionsParser&&) = delete;

		[[nodiscard]] Tools::Expected<std::optional<Options>> Parse(int argc, const char** argv) const;
		void ShowExplanation(std::wostream* emptyOptionsExplanation, const char* message) const;

		std::shared_ptr<Tools::WarningManager> optionalWarningManager_;
		std::vector<std::unique_ptr<IOptionParser>> optionParsers_;
	};
}

