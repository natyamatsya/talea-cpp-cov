// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2014 OpenCppCoverage

#include "stdafx.h"
#include "OptionsExport.hpp"

#include <map>
#include <iostream>

namespace TaleaCppCov
{
	//-------------------------------------------------------------------------
	OptionsExport::OptionsExport(OptionsExportType type,
	                             std::wstring&& name,
	                             std::optional<std::wstring>&& parameter)
	    : type_{type}, name_{std::move(name)}, parameter_{std::move(parameter)}
	{
	}

	//-------------------------------------------------------------------------
	OptionsExportType OptionsExport::GetType() const
	{
		return type_;
	}

	//-------------------------------------------------------------------------
	const std::wstring& OptionsExport::GetName() const
	{
		return name_;
	}

	//-------------------------------------------------------------------------
	const std::optional<std::wstring>& OptionsExport::GetParameter() const
	{
		return parameter_;
	}

	//-------------------------------------------------------------------------
	std::wostream& operator<<(std::wostream& ostr,
	                          const OptionsExport& optionsExport)
	{
		ostr << optionsExport.GetName();

		auto parameter = optionsExport.GetParameter();

		if (parameter)
			ostr << " " << *parameter;
		return ostr;
	}
}
