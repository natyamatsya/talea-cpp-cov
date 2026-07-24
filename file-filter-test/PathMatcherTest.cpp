// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2016 OpenCppCoverage

#include "stdafx.h"

#include <optional>

#include "file-filter/PathMatcher.hpp"
#include "file-filter/File.hpp"
#include "Tools.hpp"

using namespace FileFilter;
namespace fs = std::filesystem;

namespace std
{
	namespace filesystem
	{
		//---------------------------------------------------------------------
		void PrintTo(const path& path, ::std::ostream* os) {
			if (os)
				*os << path.string();
		}
	}
}

namespace FileFilterTest
{		
	namespace
	{
		//-------------------------------------------------------------------------
		std::wstring Match(PathMatcher& pathMatcher, const fs::path& path)
		{
			const auto* file = pathMatcher.Match(path).value();

			return (file) ? file->GetPath().wstring() : L"No file found";
		}
	}

	//-------------------------------------------------------------------------
	TEST(PathMatcherTest, PostFixBasicMatch)
	{
		std::vector<std::wstring> filenames = { L"Test.txt", L"Test\\Test2.txt" };
		auto files = ToFiles(filenames);
		PathMatcher pathMatcher{std::move(files), std::nullopt };

		ASSERT_EQ(filenames.at(0), Match(pathMatcher, "Test.txt"));
		ASSERT_EQ(filenames.at(1), Match(pathMatcher, "Test\\Test\\Test2.txt"));
		ASSERT_EQ(filenames.at(1), Match(pathMatcher, "test\\test\\test2.txt"));

		ASSERT_EQ(nullptr, pathMatcher.Match("Test2\\Test2.txt").value());
		ASSERT_EQ(nullptr, pathMatcher.Match("Test2.txt").value());
		ASSERT_EQ(nullptr, pathMatcher.Match("Test3.txt").value());
		ASSERT_EQ(0, pathMatcher.GetUnmatchedPaths().size());
	}	

	//-------------------------------------------------------------------------
	TEST(PathMatcherTest, PostFixAmgigousPath)
	{
		std::vector<File> files;
		const fs::path filename{ L"test.txt" };
		const fs::path test1{ L"test1" / filename };
		const fs::path test2{ L"test2" / filename };
		files.emplace_back(filename);
		
		PathMatcher pathMatcher{ std::move(files), std::nullopt };

		Match(pathMatcher, test1);

		auto result = pathMatcher.Match(test2);
		ASSERT_FALSE(result);
		ASSERT_EQ(filename, result.error().postFixPath);
		ASSERT_EQ(test1, result.error().firstPossiblePath);
		ASSERT_EQ(test2, result.error().secondPossiblePath);
	}
	
	//-------------------------------------------------------------------------
	TEST(PathMatcherTest, PostFixGetUnmatchedPaths)
	{		
		std::vector<std::wstring> filenames{ L"test1", L"test2", L"test\\test2", L"test3" };
		auto files = ToFiles(filenames);

		PathMatcher pathMatcher{ std::move(files), std::nullopt };

		[[maybe_unused]] auto matched = pathMatcher.Match(filenames.at(1));
		auto unmatchedPaths = pathMatcher.GetUnmatchedPaths();
		ASSERT_EQ(3, unmatchedPaths.size());
		ASSERT_EQ(filenames.at(0), unmatchedPaths.at(0).wstring());
		ASSERT_EQ(filenames.at(2), unmatchedPaths.at(1).wstring());
		ASSERT_EQ(filenames.at(3), unmatchedPaths.at(2).wstring());
	}

	//-------------------------------------------------------------------------
	TEST(PathMatcherTest, FullPathBasicMatch)
	{
		std::vector<std::wstring> filenames{ L"test.txt", L"test2" };
		auto files = ToFiles(filenames);
		fs::path parentPath = "test";
		PathMatcher pathMatcher{ std::move(files), parentPath };

		ASSERT_EQ(nullptr, pathMatcher.Match(filenames.at(0)).value());
		ASSERT_EQ(filenames.at(0), Match(pathMatcher, parentPath / filenames.at(0)));

		auto unmatchedPaths = pathMatcher.GetUnmatchedPaths();
		ASSERT_EQ(1, unmatchedPaths.size());
		ASSERT_EQ(parentPath / filenames.at(1), unmatchedPaths.at(0));
	}
}