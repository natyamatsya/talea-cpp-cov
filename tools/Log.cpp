// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2014 OpenCppCoverage

#include "stdafx.h"
#include "Log.hpp"

#include "tools/Tool.hpp"

// spdlog is included only here, so the logging backend stays confined to
// Tools.dll and links statically. Reuse our (already static) fmt rather than
// spdlog's bundled copy.
#ifndef SPDLOG_COMPILED_LIB
#define SPDLOG_COMPILED_LIB
#endif
#ifndef SPDLOG_FMT_EXTERNAL
#define SPDLOG_FMT_EXTERNAL
#endif
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
		// The single process-wide logger, living in Tools.dll. It starts with a
		// default console (stderr) sink so messages logged before
		// InitConsoleAndFileLog runs - e.g. option-parsing warnings emitted
		// while --quiet is not yet in effect - still appear, matching
		// boost::log's default-sink behaviour. InitConsoleAndFileLog /
		// InitLoggerOstream replace it.
		spdlog::logger& Logger()
		{
			static spdlog::logger logger = []
			{
				auto sink = std::make_shared<spdlog::sinks::stderr_sink_mt>();
				sink->set_pattern("[%l] %v");
				spdlog::logger created{"talea", std::move(sink)};
				created.flush_on(spdlog::level::trace);
				return created;
			}();
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
	bool ShouldLog(LogLevel level)
	{
		return Logger().should_log(ToSpdlogLevel(level));
	}

	//-------------------------------------------------------------------------
	void LogMessage(LogLevel level, std::string_view message)
	{
		// message is already-formatted UTF-8 text, not a format string.
		Logger().log(ToSpdlogLevel(level), message);
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
