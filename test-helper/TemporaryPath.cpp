// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2014 OpenCppCoverage

#include "stdafx.h"
#include <cstdint>
#include <system_error>
#include <random>

#include "TemporaryPath.hpp"

#include "tools/Log.hpp"
#include "test-helper/Tools.hpp"
#include "tools/Tool.hpp"

namespace fs = std::filesystem;

namespace TestHelper
{
	//-------------------------------------------------------------------------	
	TemporaryPath::TemporaryPath(TemporaryPathOption temporaryPathOption)
	{
		auto now = std::chrono::system_clock::now().time_since_epoch();
		auto seed = static_cast<std::uint32_t>(now.count());
		std::default_random_engine generator(seed);
		std::uniform_int_distribution<std::int32_t> distribution;
		auto temp_directory = fs::temp_directory_path();

		do
		{
			path_ = fs::absolute(temp_directory / std::to_string(distribution(generator)));
		} while (Tools::FileExists(path_));

		if (temporaryPathOption == TemporaryPathOption::CreateAsFolder)
			fs::create_directories(path_);
		else if (temporaryPathOption == TemporaryPathOption::CreateAsFile)		
			CreateEmptyFile(path_);
	}

	//-------------------------------------------------------------------------
	TemporaryPath::~TemporaryPath()
	{
		std::error_code error;
		if (!fs::remove_all(path_, error))
		{
			LOG_ERROR("{}", error.message());
		}
	}

	//-------------------------------------------------------------------------
	TemporaryPath::operator const std::filesystem::path& () const
	{
		return GetPath();
	}

	//-------------------------------------------------------------------------
	const std::filesystem::path& TemporaryPath::GetPath() const
	{
		return path_;
	}

	//-------------------------------------------------------------------------
	const std::filesystem::path* TemporaryPath::operator->() const
	{
		return &path_;
	}
}
