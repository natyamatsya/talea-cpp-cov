// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2018 OpenCppCoverage

#include "stdafx.h"
#include <system_error>

#include "FileSystem.hpp"
#include <optional>

namespace TaleaCppCov
{
	//----------------------------------------------------------------------------
	std::optional<std::filesystem::file_time_type>
	FileSystem::GetLastWriteTime(const std::filesystem::path& path) const
	{
		std::error_code error;
		auto time = std::filesystem::last_write_time(path, error);

		return error ? std::optional<std::filesystem::file_time_type>{} : time;
	}
}
