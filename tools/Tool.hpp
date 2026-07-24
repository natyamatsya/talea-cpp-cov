// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2014 OpenCppCoverage

#pragma once

#include <string>
#include <functional>

#include <filesystem>
#include <optional>

#include "ToolsExport.hpp"
#include "Error.hpp"

namespace Tools
{
	TOOLS_DLL std::wstring LocalToWString(const std::string&);
	TOOLS_DLL std::wstring Utf8ToWString(const std::string&);
	TOOLS_DLL std::string ToLocalString(const std::wstring&);
	TOOLS_DLL std::string ToUtf8String(const std::wstring&);

	TOOLS_DLL std::optional<std::wstring> Try(std::function<void()>);	
	
	template <typename Exception, typename Fct>
	decltype(auto)
	Try(Fct fct, std::function<std::string(const std::string&)> buildErrorMsg)
	{
		std::string error;
		try
		{
			return fct();
		}
		catch (const std::exception& e)
		{
			error = e.what();
		}
		catch (...)
		{
			error = "Unknow";
		}

		throw Exception(buildErrorMsg(error));
	}

	[[nodiscard]] TOOLS_DLL Expected<std::filesystem::path> GetExecutableFolder();


	TOOLS_DLL void ShowOutputMessage(const std::wstring& message, const std::filesystem::path& path);
	TOOLS_DLL std::wstring GetSeparatorLine();

	[[nodiscard]] TOOLS_DLL Expected<void> CreateParentFolderIfNeeded(const std::filesystem::path& path);
	TOOLS_DLL bool FileExists(const std::filesystem::path& path);
}


