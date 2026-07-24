// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2018 OpenCppCoverage

#pragma once

#include <optional>
#include <ctime>
#include <filesystem>

namespace TaleaCppCov
{
	class IFileSystem
	{
	  public:
		virtual ~IFileSystem() = default;

		virtual std::optional<std::filesystem::file_time_type>
		GetLastWriteTime(const std::filesystem::path&) const = 0;
	};
}
