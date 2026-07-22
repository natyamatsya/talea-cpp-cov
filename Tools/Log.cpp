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

#include "stdafx.h"
#include "Log.hpp"

#include "Tools/Tool.hpp"

// spdlog is included only here, so the logging backend stays confined to
// Tools.dll and links statically. Reuse our (already static) fmt rather than
// spdlog's bundled copy.
#define SPDLOG_COMPILED_LIB
#define SPDLOG_FMT_EXTERNAL
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/ostream_sink.h>
#include <spdlog/sinks/stdout_sinks.h>

namespace Tools
{
	namespace
	{
		LogLevel g_minSeverity = LogLevel::Info;
		bool g_enabled = true;

		//-------------------------------------------------------------------------
		spdlog::level::level_enum ToSpdlogLevel(LogLevel level)
		{
			switch (level)
			{
				case LogLevel::Trace: return spdlog::level::trace;
				case LogLevel::Debug: return spdlog::level::debug;
				case LogLevel::Info: return spdlog::level::info;
				case LogLevel::Warning: return spdlog::level::warn;
				case LogLevel::Error: return spdlog::level::err;
			}
			return spdlog::level::info;
		}

		//-------------------------------------------------------------------------
		// The single process-wide logger, living in Tools.dll. It starts with no
		// sinks (logging goes nowhere until InitConsoleAndFileLog or
		// InitLoggerOstream installs one), mirroring the previous behaviour.
		spdlog::logger& Logger()
		{
			static spdlog::logger logger{"occ"};
			return logger;
		}

		//-------------------------------------------------------------------------
		void ApplyLevel()
		{
			Logger().set_level(g_enabled ? ToSpdlogLevel(g_minSeverity)
			                             : spdlog::level::off);
		}

		//-------------------------------------------------------------------------
		void SetSink(spdlog::sink_ptr sink)
		{
			auto& sinks = Logger().sinks();
			sinks.clear();
			sinks.push_back(std::move(sink));
		}
	}

	//-------------------------------------------------------------------------
	LogStream::LogStream(LogLevel level) : level_{level}
	{
	}

	//-------------------------------------------------------------------------
	LogStream::~LogStream()
	{
		const auto level = ToSpdlogLevel(level_);
		auto& logger = Logger();
		if (!logger.should_log(level))
			return;
		// The message is a literal (never a format string), so pass it through
		// the non-formatting overload.
		logger.log(level, Tools::ToUtf8String(stream_.str()));
	}

	//-------------------------------------------------------------------------
	void InitConsoleAndFileLog(const std::filesystem::path& logPath)
	{
		// Console goes to stderr as "[severity] message"; the file adds a
		// timestamp, matching the previous boost::log output.
		auto consoleSink = std::make_shared<spdlog::sinks::stderr_sink_mt>();
		consoleSink->set_pattern("[%l] %v");

		auto fileSink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(
		    logPath.string(), /*truncate*/ true);
		fileSink->set_pattern("[%Y-%m-%d %H:%M:%S] [%l] %v");

		auto& sinks = Logger().sinks();
		sinks.clear();
		sinks.push_back(std::move(consoleSink));
		sinks.push_back(std::move(fileSink));
		Logger().flush_on(spdlog::level::trace);
		ApplyLevel();
	}

	//-------------------------------------------------------------------------
	void SetLoggerMinSeverity(LogLevel minSeverity)
	{
		g_minSeverity = minSeverity;
		ApplyLevel();
	}

	//-------------------------------------------------------------------------
	void EnableLogger(bool isEnabled)
	{
		g_enabled = isEnabled;
		ApplyLevel();
	}

	//-------------------------------------------------------------------------
	void InitLoggerOstream(const std::shared_ptr<std::ostringstream>& ostr)
	{
		// Capture just the message (no decoration), as the previous
		// text_ostream_backend did.
		auto sink =
		    std::make_shared<spdlog::sinks::ostream_sink_mt>(*ostr, /*flush*/ true);
		sink->set_pattern("%v");
		SetSink(std::move(sink));
		Logger().flush_on(spdlog::level::trace);
		ApplyLevel();
	}
}
