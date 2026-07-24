// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2014 OpenCppCoverage

#pragma once

#include <optional>

#include "TaleaCppCovExport.hpp"

namespace TaleaCppCov
{
	enum class OptionsExportType
	{
		Html,
		Cobertura,
		Binary
	};

	class TALEACPPCOV_DLL OptionsExport
	{
	  public:
		explicit OptionsExport(OptionsExportType type,
		                       std::wstring&& name,
		                       std::optional<std::wstring>&& argument);

		OptionsExport(OptionsExport&&) = default;
		OptionsExport& operator=(OptionsExport&&) = default;

		OptionsExport(const OptionsExport&) = delete;
		OptionsExport& operator=(const OptionsExport&) = delete;
		
		OptionsExportType GetType() const;
		const std::wstring& GetName() const;
		const std::optional<std::wstring>& GetParameter() const;

		friend std::wostream& operator<<(std::wostream& ostr,
		                                 const OptionsExport&);

	  private:
		OptionsExportType type_;
		std::wstring name_;
		std::optional<std::wstring> parameter_;
	};
}
