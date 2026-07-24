// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2018 OpenCppCoverage

#include "stdafx.h"
#include "InvalidOutputFileException.hpp"

namespace Exporter
{
	namespace
	{
		//---------------------------------------------------------------------
		std::string GetErrorMessage(const std::filesystem::path& output,
		                            const std::string& outputKind)
		{
			auto error = "Cannot write " + outputKind +
			             " export to the file: \"" + output.string() + "\".";

			if (std::filesystem::is_directory(output))
			{
				error += " The output \"" + output.string() +
				         "\" is a directory whereas as it must be a file.";
			}
			return error;
		}
	}

	//-------------------------------------------------------------------------
	InvalidOutputFileException::InvalidOutputFileException(
	    const std::filesystem::path& output, const std::string& outputKind)
	    : std::runtime_error(GetErrorMessage(output, outputKind))
	{
	}
}
