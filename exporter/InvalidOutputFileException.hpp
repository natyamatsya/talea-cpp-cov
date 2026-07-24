// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2018 OpenCppCoverage

#pragma once

#include <stdexcept>
#include <filesystem>

#include "ExporterExport.hpp"

namespace Exporter
{
#pragma warning(push)
#pragma warning(disable : 4275) // non dll-interface class 'std::runtime_error'
                                // used as base for dll-interface class
                                // 'Exporter::InvalidOutputFileException'

	class EXPORTER_DLL InvalidOutputFileException : public std::runtime_error
	{
	  public:
		InvalidOutputFileException(const std::filesystem::path& output,
		                           const std::string& outputKind);
	};
#pragma warning(pop)
}
