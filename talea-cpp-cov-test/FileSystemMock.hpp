// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2018 OpenCppCoverage

#pragma once

#include "talea-cpp-cov/IFileSystem.hpp"

namespace TaleaCppCovTest
{
	class FileSystemMock : public TaleaCppCov::IFileSystem
	{
	  public:
		MOCK_CONST_METHOD1(
		    GetLastWriteTime,
		    std::optional<std::filesystem::file_time_type>(const std::filesystem::path&));
	};
}