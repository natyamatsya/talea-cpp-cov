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

#include "ToolsExport.hpp"

namespace Tools
{
	enum class LogLevel
	{
		Trace,
		Debug,
		Info,
		Warning,
		Error
	};

	// A stream-style front end for the logger. Each LOG_* use builds one
	// temporary that accumulates a wide message and, on destruction, converts
	// it to UTF-8 and emits a single line through the logger that lives in
	// Tools.dll. Because only Tools/Log.cpp includes the logging backend
	// (spdlog), the backend stays confined to one module and links statically.
	//
	// This adapter is the transitional bridge that keeps the existing
	// "LOG_X << ..." call sites working; they are being converted to format
	// strings in batches, after which this class is removed. See
	// docs/SPDLOG_MIGRATION.md.
	class TOOLS_DLL LogStream
	{
	public:
		explicit LogStream(LogLevel level);
		~LogStream();

		template <typename T>
		LogStream& operator<<(const T& value)
		{
			stream_ << value;
			return *this;
		}

		// Collections keep their historical "[a, b, c]" rendering (previously a
		// boost::log operator<< for std::set).
		template <typename T>
		LogStream& operator<<(const std::set<T>& values)
		{
			stream_ << L'[';
			bool first = true;
			for (const auto& value : values)
			{
				if (!first)
					stream_ << L", ";
				first = false;
				stream_ << value;
			}
			stream_ << L']';
			return *this;
		}

	private:
		LogStream(const LogStream&) = delete;
		LogStream& operator=(const LogStream&) = delete;

		LogLevel level_;
		std::wostringstream stream_;
	};

	void TOOLS_DLL InitConsoleAndFileLog(const std::filesystem::path& logPath);
	void TOOLS_DLL SetLoggerMinSeverity(LogLevel minSeverity);
	void TOOLS_DLL EnableLogger(bool isEnabled);
	void TOOLS_DLL InitLoggerOstream(const std::shared_ptr<std::ostringstream>& ostr);
}

#define LOG_TRACE   ::Tools::LogStream{::Tools::LogLevel::Trace}
#define LOG_DEBUG   ::Tools::LogStream{::Tools::LogLevel::Debug}
#define LOG_INFO    ::Tools::LogStream{::Tools::LogLevel::Info}
#define LOG_WARNING ::Tools::LogStream{::Tools::LogLevel::Warning}
#define LOG_ERROR   ::Tools::LogStream{::Tools::LogLevel::Error}
