// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2014 OpenCppCoverage

#pragma once

#include <string>
#include <vector>
#include <iosfwd>
#include <optional>
#include <filesystem>

#include "TaleaCppCovExport.hpp"

namespace TaleaCppCov
{
	class TALEACPPCOV_DLL StartInfo
	{
	public:
		explicit StartInfo(const std::filesystem::path&);
		
		StartInfo(StartInfo&&);

		StartInfo(const StartInfo&) = default;		
		StartInfo& operator=(const StartInfo&) = default;

		void SetWorkingDirectory(const std::filesystem::path&);
		void AddArgument(const std::wstring&);

		const std::filesystem::path& GetPath() const;
		const std::vector<std::wstring>& GetArguments() const;
		const std::filesystem::path* GetWorkingDirectory() const;

		friend TALEACPPCOV_DLL std::wostream& operator<<(std::wostream& ostr, const StartInfo&);

	private:
		std::filesystem::path path_;
		std::vector<std::wstring> arguments_;
		std::optional<std::filesystem::path> workingDirectory_;
	};
}


