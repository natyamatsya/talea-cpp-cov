// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2019 OpenCppCoverage

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
