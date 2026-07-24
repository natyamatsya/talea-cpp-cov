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

