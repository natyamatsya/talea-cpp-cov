// OpenCppCoverage is an open source code coverage for C++.
// Copyright (C) 2026 OpenCppCoverage
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

#include <array>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "CppCoverage/GlazeCli.hpp"

namespace
{
	struct Spec
	{
		bool flag = false;
		std::optional<int> threads;
		std::optional<bool> logging;
		std::string name;
		std::string program;
		std::vector<std::string> items;

		static constexpr std::array shorts = {
		    std::pair{'f', std::string_view{"flag"}}};
		static constexpr std::array<std::string_view, 1> positionals = {
		    "program"};
		static constexpr std::array descriptions = {
		    std::pair{std::string_view{"flag"}, std::string_view{"a flag"}}};
	};
}

namespace glz
{
	template <>
	struct meta<Spec>
	{
		using T = Spec;
		static constexpr auto value = object("flag",
		                                     &T::flag,
		                                     "threads",
		                                     &T::threads,
		                                     "logging",
		                                     &T::logging,
		                                     "name",
		                                     &T::name,
		                                     "program",
		                                     &T::program,
		                                     "items",
		                                     &T::items);
	};
}

namespace
{
	glzcli::ParseResult Parse(Spec& spec,
	                          const std::vector<std::string>& args)
	{
		return glzcli::parse(spec, std::span<const std::string>{args});
	}
}

namespace CppCoverage
{
	TEST(GlazeCliTest, FlagValueVectorAndPositional)
	{
		Spec spec;
		auto result = Parse(spec,
		                    {"--flag",
		                     "--threads",
		                     "4",
		                     "--name",
		                     "abc",
		                     "--items",
		                     "a,b",
		                     "--items",
		                     "c",
		                     "prog.exe"});

		ASSERT_FALSE(result.error.has_value());
		EXPECT_TRUE(spec.flag);
		ASSERT_TRUE(spec.threads.has_value());
		EXPECT_EQ(4, *spec.threads);
		EXPECT_EQ("abc", spec.name);
		EXPECT_EQ("prog.exe", spec.program);
		ASSERT_EQ(3u, spec.items.size());
		EXPECT_EQ("a", spec.items[0]);
		EXPECT_EQ("b", spec.items[1]);
		EXPECT_EQ("c", spec.items[2]);
	}

	TEST(GlazeCliTest, ShortAliasEqualsFormAndBoolValue)
	{
		Spec spec;
		auto result = Parse(spec, {"-f", "--threads=8", "--logging", "false"});

		ASSERT_FALSE(result.error.has_value());
		EXPECT_TRUE(spec.flag);
		ASSERT_TRUE(spec.threads.has_value());
		EXPECT_EQ(8, *spec.threads);
		ASSERT_TRUE(spec.logging.has_value());
		EXPECT_FALSE(*spec.logging);
	}

	TEST(GlazeCliTest, UnknownOptionAndBadNumberAreErrors)
	{
		Spec unknown;
		EXPECT_TRUE(Parse(unknown, {"--nope"}).error.has_value());

		Spec badNumber;
		EXPECT_TRUE(Parse(badNumber, {"--threads", "abc"}).error.has_value());
	}

	TEST(GlazeCliTest, HelpIsDetectedAndHelpTextRenders)
	{
		Spec spec;
		EXPECT_TRUE(Parse(spec, {"--help"}).help);
		EXPECT_NE(std::string::npos, glzcli::help<Spec>().find("--flag"));
	}
}
