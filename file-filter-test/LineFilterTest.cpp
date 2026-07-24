// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2017 OpenCppCoverage

#include "stdafx.h"

#include "file-filter/LineFilter.hpp"
#include "test-helper/TemporaryPath.hpp"

using namespace FileFilter;

namespace FileFilterTest
{	
	namespace
	{
		const auto line1 = __LINE__;
		const auto line2 = __LINE__;
		const auto line21 = __LINE__;
	}

	//-------------------------------------------------------------------------
	TEST(LineFilterTest, SingleRegex)
	{
		LineFilter filter{ {L".*li.*2 =.*"} };

		ASSERT_TRUE(filter.IsLineSelected(__FILE__, line1));
		ASSERT_FALSE(filter.IsLineSelected(__FILE__, line2));
		ASSERT_TRUE(filter.IsLineSelected(__FILE__, line21));
		ASSERT_EQ(1, filter.GetFileReadCount());
	}

	//-------------------------------------------------------------------------
	TEST(LineFilterTest, SeveralRegex)
	{
		LineFilter filter{ { L".*line1.*", L".*line21.*" } };

		ASSERT_FALSE(filter.IsLineSelected(__FILE__, line1));
		ASSERT_TRUE(filter.IsLineSelected(__FILE__, line2));
		ASSERT_FALSE(filter.IsLineSelected(__FILE__, line21));
	}

	//-------------------------------------------------------------------------
	TEST(LineFilterTest, FileNotFound)
	{
		LineFilter filter{ {L".*"} };

		ASSERT_TRUE(filter.IsLineSelected(L"FileNotFound", line1));
	}

	//-------------------------------------------------------------------------
	TEST(LineFilterTest, NoRegex)
	{
		LineFilter filter{ {} };

		ASSERT_TRUE(filter.IsLineSelected(__FILE__, line1));
	}

	//-------------------------------------------------------------------------
	TEST(LineFilterTest, InvalidLine)
	{
		bool enableLog = false;
		LineFilter filter{ { L"" }, enableLog };

		ASSERT_FALSE(filter.IsLineSelected(__FILE__, 1000 * 1000));
	}

	//-------------------------------------------------------------------------
	TEST(LineFilterTest, NoPartialMatch)
	{
		LineFilter filter{ { L"line" } };

		ASSERT_TRUE(filter.IsLineSelected(__FILE__, line1));
	}

	//-------------------------------------------------------------------------
	TEST(LineFilterTest, GetFileReadCount)
	{
		LineFilter filter{ { L".*line.*" } };

		ASSERT_FALSE(filter.IsLineSelected(__FILE__, line1));
		
		TestHelper::TemporaryPath path{ TestHelper::TemporaryPathOption::CreateAsFile };
		ASSERT_TRUE(filter.IsLineSelected(path.GetPath(), line1));

		ASSERT_FALSE(filter.IsLineSelected(__FILE__, line1));
		ASSERT_EQ(2, filter.GetFileReadCount());
	}
}