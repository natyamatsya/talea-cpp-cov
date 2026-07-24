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
#include <cstdint>

#include <array>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "talea-cpp-cov/GlazeCli.hpp"
#include "talea-cpp-cov/OptionsSpec.hpp"

namespace
{
	struct Spec
	{
		bool flag = false;
		std::optional<std::int32_t> threads;
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

namespace TaleaCppCov
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

	//-------------------------------------------------------------------------
	// The real OptionsSpec: a representative command line and JSON round-trip.
	TEST(OptionsSpecTest, ParsesRepresentativeCommandLine)
	{
		OptionsSpec spec;
		std::vector<std::string> args = {"--sources",
		                                 "MyProject",
		                                 "--modules",
		                                 "a.dll,b.dll",
		                                 "--branch_coverage",
		                                 "-q",
		                                 "--export_type",
		                                 "cobertura:out.xml",
		                                 "--input_coverage",
		                                 "old.cov",
		                                 "--working_dir",
		                                 "C:\\work"};
		auto result =
		    glzcli::parse(spec, std::span<const std::string>{args});

		ASSERT_FALSE(result.error.has_value());
		ASSERT_EQ(1u, spec.sources.size());
		EXPECT_EQ("MyProject", spec.sources[0]);
		ASSERT_EQ(2u, spec.modules.size());
		EXPECT_EQ("b.dll", spec.modules[1]);
		EXPECT_TRUE(spec.branch_coverage);
		EXPECT_TRUE(spec.quiet);
		EXPECT_FALSE(spec.verbose);
		ASSERT_EQ(1u, spec.export_type.size());
		EXPECT_EQ("cobertura:out.xml", spec.export_type[0]);
		ASSERT_EQ(1u, spec.input_coverage.size());
		ASSERT_TRUE(spec.working_dir.has_value());
		EXPECT_EQ("C:\\work", *spec.working_dir);
	}

	TEST(OptionsSpecTest, AcceptsKebabAndSnakeCase)
	{
		OptionsSpec kebab;
		auto k = glzcli::parse(
		    kebab,
		    std::span<const std::string>{std::vector<std::string>{
		        "--no-cover-children", "--no-pdb-cache", "--working-dir", "d"}});
		ASSERT_FALSE(k.error.has_value());
		EXPECT_TRUE(kebab.no_cover_children);
		EXPECT_TRUE(kebab.no_pdb_cache);
		ASSERT_TRUE(kebab.working_dir.has_value());
		EXPECT_EQ("d", *kebab.working_dir);

		OptionsSpec snake;
		auto s = glzcli::parse(
		    snake,
		    std::span<const std::string>{std::vector<std::string>{
		        "--no_cover_children", "--no_pdb_cache"}});
		ASSERT_FALSE(s.error.has_value());
		EXPECT_TRUE(snake.no_cover_children);
		EXPECT_TRUE(snake.no_pdb_cache);
	}

	TEST(OptionsSpecTest, JsonConfigRoundTrips)
	{
		OptionsSpec written;
		written.branch_coverage = true;
		written.engine = "debugger";
		written.sources = {"src"};
		written.export_type = {"html:report"};
		written.working_dir = "C:\\w";

		std::string json;
		ASSERT_FALSE(glz::write_json(written, json));

		OptionsSpec read;
		ASSERT_FALSE(glz::read_json(read, json));
		EXPECT_TRUE(read.branch_coverage);
		ASSERT_TRUE(read.engine.has_value());
		EXPECT_EQ("debugger", *read.engine);
		ASSERT_EQ(1u, read.sources.size());
		EXPECT_EQ("src", read.sources[0]);
		ASSERT_EQ(1u, read.export_type.size());
		EXPECT_EQ("html:report", read.export_type[0]);
		ASSERT_TRUE(read.working_dir.has_value());
		EXPECT_EQ("C:\\w", *read.working_dir);
	}
}
