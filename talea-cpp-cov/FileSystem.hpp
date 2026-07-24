// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2018 OpenCppCoverage

#pragma once

#include "IFileSystem.hpp"

namespace TaleaCppCov
{
	class FileSystem : public IFileSystem
	{
	  public:
		std::optional<std::filesystem::file_time_type>
		GetLastWriteTime(const std::filesystem::path&) const override;
	};
}
