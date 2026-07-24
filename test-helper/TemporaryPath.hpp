// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2014 OpenCppCoverage

#pragma once

#include <filesystem>

#include "TestHelperExport.hpp"

namespace TestHelper
{
	enum class TemporaryPathOption
	{
		DoNotCreate,
		CreateAsFile,
		CreateAsFolder
	};

	class TEST_HELPER_DLL TemporaryPath
	{
	public:
		 
		explicit TemporaryPath(TemporaryPathOption = TemporaryPathOption::DoNotCreate);
		~TemporaryPath();

		operator const std::filesystem::path& () const;
		const std::filesystem::path& GetPath() const;
		const std::filesystem::path* operator->() const;

	private:
		TemporaryPath(const TemporaryPath&) = delete;
		TemporaryPath& operator=(const TemporaryPath&) = delete;

	private:
		std::filesystem::path path_;
	};
}