// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2016 OpenCppCoverage

#pragma once
#include <cstdint>

#include "FileFilterExport.hpp"
#include <set>
#include <filesystem>

namespace FileFilter
{
	class FILEFILTER_DLL File
	{
	public:
		explicit File(const std::filesystem::path&);
		File(File&&) = default;
		File& operator=(File&&) = default;

		void AddSelectedLines(const std::vector<std::int32_t>&);

		const std::filesystem::path& GetPath() const;
		void SetPath(const std::filesystem::path&);

		bool IsLineSelected(std::int32_t) const;
		const std::set<std::int32_t>& GetSelectedLines() const;

		bool operator==(const File&) const;

	private:
		File(const File&) = delete;
		File& operator=(const File&) = delete;

		std::filesystem::path path_;
		std::set<std::int32_t> selectedLines_;
	};
}

