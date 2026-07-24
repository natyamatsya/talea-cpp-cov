// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 natyamatsya and talea-cpp-cov contributors

#include "stdafx.h"

#include <filesystem>
#include <memory>
#include <set>
#include <sstream>
#include <string>

#include "tools/Log.hpp"

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
