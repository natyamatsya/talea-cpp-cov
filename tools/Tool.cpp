// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2014 OpenCppCoverage

#include "stdafx.h"
#include <cstdint>
#include "Tool.hpp"

#include <optional>
#include <filesystem>
#include <system_error>
#include <stdexcept>

#include "Log.hpp"

namespace fs = std::filesystem;

namespace Tools
{
	namespace
	{
		//-----------------------------------------------------------------------------
		Expected<fs::path> GetExecutablePath()
		{
			const std::int32_t PathBufferSize = 40 * 1000;
			std::vector<wchar_t> filename(PathBufferSize);

			if (!GetModuleFileName(nullptr, &filename[0], static_cast<std::int32_t>(filename.size())))
				return std::unexpected(MAKE_ERROR("Cannot get current executable path."));

			return fs::path{ &filename[0] };
		}
		
		//-------------------------------------------------------------------------
		std::string ToString(std::uint32_t pageCode, const std::wstring& str)
		{
			if (str.empty())
				return{};

			auto size = WideCharToMultiByte(pageCode, 0, str.c_str(),
				static_cast<std::int32_t>(str.size()), nullptr, 0, nullptr, nullptr);
			std::vector<char> buffer(size);

			if (!WideCharToMultiByte(pageCode, 0, str.c_str(), static_cast<std::int32_t>(str.size()),
				&buffer[0], static_cast<std::int32_t>(buffer.size()), nullptr, nullptr))
			{
				throw std::runtime_error("Error in WideCharToMultiByte.");
			}

			return{ buffer.begin(), buffer.end() };
		}

		//-------------------------------------------------------------------------
		std::wstring ToWString(std::uint32_t pageCode, const std::string& str)
		{
			if (str.empty())
				return{};

			auto size = MultiByteToWideChar(pageCode, 0,
				str.c_str(), static_cast<std::int32_t>(str.size()), nullptr, 0);
			std::vector<wchar_t> buffer(size);

			if (!MultiByteToWideChar(pageCode, 0, str.c_str(), static_cast<std::int32_t>(str.size()),
				&buffer[0], static_cast<std::int32_t>(buffer.size())))
			{
				throw std::runtime_error("Error in MultiByteToWideChar for " + str);
			}

			return{ buffer.begin(), buffer.end() };
		}
	}

	//-----------------------------------------------------------------------------
	Expected<fs::path> GetExecutableFolder()
	{
		auto executablePath = GetExecutablePath();
		if (!executablePath)
			return std::unexpected(std::move(executablePath.error()));

		return executablePath->parent_path();
	}

	//-------------------------------------------------------------------------
	std::string ToLocalString(const std::wstring& str)
	{
		return ToString(CP_ACP, str);
	}

	//-------------------------------------------------------------------------
	std::string ToUtf8String(const std::wstring& str)
	{
		return ToString(CP_UTF8, str);
	}

	//-------------------------------------------------------------------------
	std::wstring LocalToWString(const std::string& str)
	{
		return ToWString(CP_ACP, str);
	}

	//-------------------------------------------------------------------------
	std::wstring Utf8ToWString(const std::string& str)
	{
		return ToWString(CP_UTF8, str);
	}

	//-------------------------------------------------------------------------
	std::optional<std::wstring> Try(std::function<void()> action)
	{
		try
		{
			action();
		}
		catch (const std::exception& e)
		{
			return Tools::LocalToWString(e.what());
		}
		catch (...)
		{
			return std::wstring{L"Unkown exception"};
		}

		return std::nullopt;
	}

	//-------------------------------------------------------------------------
	void ShowOutputMessage(
		const std::wstring& message, 
		const std::filesystem::path& path)
	{
		LOG_INFO("{}", GetSeparatorLine());
		LOG_INFO("{}{}", message, path.wstring());
		LOG_INFO("{}", GetSeparatorLine());
	}

	//-------------------------------------------------------------------------
	std::wstring GetSeparatorLine()
	{
		return L"----------------------------------------------------";
	}
	
	//---------------------------------------------------------------------
	Expected<void> CreateParentFolderIfNeeded(const std::filesystem::path& path)
	{
		if (path.has_parent_path())
		{
			auto parentPath = path.parent_path();
			std::error_code er;

			std::filesystem::create_directories(parentPath, er);
			if (er)
			{
				return std::unexpected(
				    MAKE_ERROR(L"Error when creating folder " << parentPath.wstring()
				               << L" Error code:" << er.value()));
			}
		}
		return {};
	}

	//---------------------------------------------------------------------
	bool FileExists(const std::filesystem::path& path)
	{
		std::error_code ignoredErrorCode;

		// Error can happen when the drive is not ready (DVD for example).
		return std::filesystem::exists(path, ignoredErrorCode);
	}
}