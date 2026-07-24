// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2016 OpenCppCoverage

#include "stdafx.h"
#include "tools/Tool.hpp"

namespace ToolsTests
{
	//---------------------------------------------------------------------
	TEST(Tool, ToLocalString)
	{
		ASSERT_EQ("", Tools::ToLocalString(L""));
		ASSERT_EQ("123456789", Tools::ToLocalString(L"123456789"));
		ASSERT_EQ(1, Tools::ToLocalString(L"1").size());
		ASSERT_EQ(std::string("\xE9\xE0\xE8"), Tools::ToLocalString(L"\u00E9\u00E0\u00E8"));
	}

	//---------------------------------------------------------------------
	TEST(Tool, LocalToWString)
	{
		ASSERT_EQ(L"", Tools::LocalToWString(""));
		ASSERT_EQ(L"123456789", Tools::LocalToWString("123456789"));
		ASSERT_EQ(1, Tools::LocalToWString("1").size());
		ASSERT_EQ(std::wstring(L"\u00E9\u00E0\u00E8"), Tools::LocalToWString("\xE9\xE0\xE8"));
	}

	//---------------------------------------------------------------------
	TEST(Tool, Uft8)
	{
		ASSERT_EQ(L"\u00E9\u00E0\u00E8", Tools::Utf8ToWString(Tools::ToUtf8String(L"\u00E9\u00E0\u00E8")));
	}
}