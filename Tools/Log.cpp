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

#include <filesystem>
#include <locale>

#include <boost/log/expressions.hpp>
#include <boost/log/utility/setup/file.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/support/date_time.hpp>

namespace logging = boost::log;
namespace sinks = boost::log::sinks;
namespace keywords = boost::log::keywords;
namespace expr = boost::log::expressions;

namespace Tools
{
	namespace
	{
		//-------------------------------------------------------------------------
		void SetLogSink(boost::shared_ptr<logging::sinks::sink> sink)
		{
			logging::core::get()->remove_all_sinks();
			logging::core::get()->add_sink(sink);
		}
	}

	//-------------------------------------------------------------------------
	void InitConsoleAndFileLog(const std::filesystem::path& logPath)
	{		
		boost::log::add_common_attributes();

		auto fileSink = logging::add_file_log(logPath.wstring(),
			boost::log::keywords::format =
			expr::stream
			<< "[" << expr::format_date_time< boost::posix_time::ptime >("TimeStamp", "%Y-%m-%d %H:%M:%S")
			<< "] [" << logging::trivial::severity
			<< "] " << expr::message);

		auto consoleSink = logging::add_console_log(std::clog,
			boost::log::keywords::format =
			expr::stream
			<< "[" << logging::trivial::severity
			<< "] " << expr::message
			);

		// UTF-8 output so paths with special characters are not mangled. The
		// OS supplies a UTF-8 codecvt via the ".UTF-8" code-page locale
		// (Windows 10+), which avoids pulling in ICU through boost::locale. If
		// that locale is unavailable the log simply falls back to the default
		// encoding.
		try
		{
			std::locale loc(".UTF-8");
			fileSink->imbue(loc);
			consoleSink->imbue(loc);
		}
		catch (const std::exception&)
		{
		}
	}

	//-------------------------------------------------------------------------
	void SetLoggerMinSeverity(boost::log::trivial::severity_level minSeverity)
	{
		auto filter = logging::trivial::severity >= minSeverity;

		logging::core::get()->set_filter(filter);		
	}

	//-------------------------------------------------------------------------
	void EnableLogger(bool isEnabled)
	{
		logging::core::get()->set_logging_enabled(isEnabled);
	}

	//-------------------------------------------------------------------------
	void InitLoggerOstream(const boost::shared_ptr<std::ostringstream>& ostr)
	{
		typedef sinks::synchronous_sink<sinks::text_ostream_backend> text_sink;
		auto sink = boost::make_shared<text_sink>();
		auto backend = sink->locked_backend();

		backend->add_stream(ostr);
		backend->auto_flush(true);
		SetLogSink(sink);
	}	
}