// OpenCppCoverage is an open source code coverage for C++.
// Copyright (C) 2014 OpenCppCoverage
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

#pragma once

#include <filesystem>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>

#include <fmt/format.h>

#include "ToolsExport.hpp"
#include "tools/Tool.hpp"

namespace Tools
{
	// The logger and its call sites are wide (wchar_t); the backend (spdlog) is
	// UTF-8. fmt refuses wide string/char arguments in a narrow format string,
	// so wide and path arguments are converted to UTF-8 here before they reach
	// fmt; everything else is forwarded unchanged. This keeps call sites clean:
	// LOG_INFO("loaded {}", modulePath) just works.
	// Returns by value (decayed) so the mapped argument type below matches the
	// value actually handed to fmt exactly - fmt::format_string fixes the types,
	// and a decayed prvalue binds cleanly (a forwarded reference or an array
	// type would not).
	template <typename T>
	auto LogArg(T&& value)
	{
		using D = std::decay_t<T>;
		if constexpr (std::is_same_v<D, std::wstring>)
			return ToUtf8String(value);
		else if constexpr (std::is_same_v<D, std::wstring_view>)
			return ToUtf8String(std::wstring{value});
		else if constexpr (std::is_same_v<D, std::filesystem::path>)
			return ToUtf8String(value.wstring());
		else if constexpr (std::is_same_v<D, const wchar_t*> ||
		                   std::is_same_v<D, wchar_t*>)
			return ToUtf8String(std::wstring{value});
		else
			return static_cast<D>(std::forward<T>(value));
	}

	// The type an argument becomes once mapped for fmt (wide -> std::string).
	template <typename T>
	using LoggableArg = decltype(LogArg(std::declval<T>()));

	enum class LogLevel
	{
		Trace,
		Debug,
		Info,
		Warning,
		Error
	};

	// True when a message at this level would be emitted; lets Log() skip
	// formatting work when the level is filtered out.
	bool TOOLS_DLL ShouldLog(LogLevel level);

	// Emits an already-formatted UTF-8 line. Kept non-template and exported so
	// the spdlog backend stays confined to Tools.dll.
	void TOOLS_DLL LogMessage(LogLevel level, std::string_view message);

	// The format-string logging front end: LOG_INFO("loaded {}", modulePath);
	// The format string is checked against the arguments at compile time.
	template <typename... Args>
	void Log(LogLevel level,
	         fmt::format_string<LoggableArg<Args>...> format,
	         Args&&... args)
	{
		if (!ShouldLog(level))
			return;
		LogMessage(level, fmt::format(format, LogArg(std::forward<Args>(args))...));
	}

	// Renders a range as "[a, b, c]" (matching the previous std::set logging),
	// each element formatted through fmt (so wide elements convert to UTF-8).
	template <typename Range>
	std::string Join(const Range& range)
	{
		std::string result = "[";
		bool first = true;
		for (const auto& item : range)
		{
			if (!first)
				result += ", ";
			first = false;
			result += fmt::to_string(LogArg(item));
		}
		result += "]";
		return result;
	}

	void TOOLS_DLL InitConsoleAndFileLog(const std::filesystem::path& logPath);
	void TOOLS_DLL SetLoggerMinSeverity(LogLevel minSeverity);
	void TOOLS_DLL EnableLogger(bool isEnabled);
	void TOOLS_DLL InitLoggerOstream(const std::shared_ptr<std::ostringstream>& ostr);
}

// Format logging: LOG_INFO("loaded {} in {} ms", module, ms);
#define LOG_TRACE(...)   ::Tools::Log(::Tools::LogLevel::Trace, __VA_ARGS__)
#define LOG_DEBUG(...)   ::Tools::Log(::Tools::LogLevel::Debug, __VA_ARGS__)
#define LOG_INFO(...)    ::Tools::Log(::Tools::LogLevel::Info, __VA_ARGS__)
#define LOG_WARNING(...) ::Tools::Log(::Tools::LogLevel::Warning, __VA_ARGS__)
#define LOG_ERROR(...)   ::Tools::Log(::Tools::LogLevel::Error, __VA_ARGS__)
