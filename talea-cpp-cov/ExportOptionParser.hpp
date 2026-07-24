// OpenCppCoverage is an open source code coverage for C++.
// Copyright (C) 2019 OpenCppCoverage
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

#include <map>

#include "TaleaCppCovExport.hpp"
#include "IOptionParser.hpp"
#include "OptionsExport.hpp"

namespace TaleaCppCov
{
	struct OptionsSpec;

	class TALEACPPCOV_DLL ExportOptionParser : public IOptionParser
	{
	  public:
		static const char ExportSeparator;
		static const std::string ExportTypeOption;
		static const std::string ExportTypeHtmlValue;
		static const std::string ExportTypeCoberturaValue;
		static const std::string ExportTypeBinaryValue;

		ExportOptionParser();

		ExportOptionParser(const ExportOptionParser&) = delete;
		ExportOptionParser& operator=(const ExportOptionParser&) = delete;

		Tools::Expected<void> ParseOption(const OptionsSpec&, Options&) override;

	  private:
		std::wstring GetExportTypeText() const;

		std::map<std::wstring, OptionsExportType> exportTypes_;
	};
}
