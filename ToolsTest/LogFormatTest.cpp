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

#include <filesystem>
#include <memory>
#include <set>
#include <sstream>
#include <string>

#include "Tools/Log.hpp"

namespace ToolsTest
{
	// Exercises the LOG_*(...) format form and its wide/path formatters by
	// capturing the emitted text through an ostream sink.
	class LogFormatTest : public ::testing::Test
	{
	protected:
		void SetUp() override
		{
			output_ = std::make_shared<std::ostringstream>();
			Tools::EnableLogger(true);
			Tools::SetLoggerMinSeverity(Tools::LogLevel::Trace);
			Tools::InitLoggerOstream(output_);
		}

		void TearDown() override
		{
			Tools::EnableLogger(false);
		}

		std::string Captured() const
		{
			return output_->str();
		}

		std::shared_ptr<std::ostringstream> output_;
	};

	//-------------------------------------------------------------------------
	TEST_F(LogFormatTest, FormatsNarrowAndIntegers)
	{
		LOG_INFO("count={} name={}", 42, "abc");
		EXPECT_NE(Captured().find("count=42 name=abc"), std::string::npos);
	}

	//-------------------------------------------------------------------------
	TEST_F(LogFormatTest, ConvertsWideStringToUtf8)
	{
		LOG_INFO("value={}", std::wstring{L"wide"});
		EXPECT_NE(Captured().find("value=wide"), std::string::npos);
	}

	//-------------------------------------------------------------------------
	TEST_F(LogFormatTest, FormatsFilesystemPath)
	{
		const std::filesystem::path path = L"dir/file.cpp";
		LOG_ERROR("path={}", path);
		EXPECT_NE(Captured().find("file.cpp"), std::string::npos);
	}

	//-------------------------------------------------------------------------
	TEST_F(LogFormatTest, JoinRendersSetWithBrackets)
	{
		const std::set<std::wstring> values{L"a", L"b", L"c"};
		LOG_INFO("items={}", Tools::Join(values));
		EXPECT_NE(Captured().find("items=[a, b, c]"), std::string::npos);
	}

	//-------------------------------------------------------------------------
	TEST_F(LogFormatTest, SeverityFilterSkipsLowerLevels)
	{
		Tools::SetLoggerMinSeverity(Tools::LogLevel::Error);
		LOG_INFO("should-not-appear");
		LOG_ERROR("should-appear");

		const auto captured = Captured();
		EXPECT_EQ(captured.find("should-not-appear"), std::string::npos);
		EXPECT_NE(captured.find("should-appear"), std::string::npos);
	}

}
