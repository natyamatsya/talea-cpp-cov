// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2017 OpenCppCoverage

#pragma once

#include <vector>
#include <string>
#include <memory>
#include <filesystem>

#include "ToolsExport.hpp"
#include "Error.hpp"

namespace Tools
{
	class TOOLS_DLL MappedFile
	{
	public:
		// Returns the mapped file, a null pointer when the path is missing or
		// empty (not an error), or a Tools::Error when an existing file cannot
		// be mapped.
		[[nodiscard]] static Expected<std::unique_ptr<MappedFile>>
		TryCreate(const std::filesystem::path&);

		const std::vector<std::string>& GetLines() const;

		MappedFile(const MappedFile&) = delete;
		MappedFile& operator=(const MappedFile&) = delete;
		MappedFile(MappedFile&&) = default;

	private:
		explicit MappedFile(std::vector<std::string> lines);
		static Expected<std::vector<std::string>>
		LoadLines(const std::filesystem::path&);

		std::vector<std::string> lines_;
	};
}