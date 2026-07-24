// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2017 OpenCppCoverage

#include "stdafx.h"
#include <cstdint>
#include "tools/MappedFile.hpp"
#include <fstream>

#include "test-helper/TemporaryPath.hpp"

namespace ToolsTests
{
	namespace
	{
		//---------------------------------------------------------------------
		std::unique_ptr<TestHelper::TemporaryPath> CreateFile(const std::vector<std::string>& lines)
		{
			auto path = std::make_unique<TestHelper::TemporaryPath>();
			std::ofstream ofs(path->GetPath().string(), std::ios::binary);

			for (const auto& line : lines)
				ofs.write(line.c_str(), line.size());
			return path;
		}

		//---------------------------------------------------------------------
		std::vector<std::string> GetLines(const std::filesystem::path& path)
		{
			std::ifstream ifs(path.wstring());
			std::vector<std::string> lines;
			std::string line;

			while (std::getline(ifs, line))
				lines.push_back(line);
			return lines;
		}
	}

	//---------------------------------------------------------------------
	TEST(MappedFileTest, GetLine)
	{
		auto path = CreateFile({ "\r\n", "abc\r\n", "123\n" });
		auto file = Tools::MappedFile::TryCreate(path->GetPath()).value();
		auto expectedLines = GetLines(*path);
		ASSERT_EQ(expectedLines, file->GetLines());
	}

	//---------------------------------------------------------------------
	TEST(MappedFileTest, EmptyFile)
	{
		TestHelper::TemporaryPath path{ TestHelper::TemporaryPathOption::CreateAsFile };
		ASSERT_EQ(nullptr, Tools::MappedFile::TryCreate(path.GetPath()).value());
	}

	//---------------------------------------------------------------------
	TEST(MappedFileTest, MissingFile)
	{
		ASSERT_EQ(nullptr, Tools::MappedFile::TryCreate("MissingFile").value());
	}

	//---------------------------------------------------------------------
	TEST(MappedFileTest, WholeFile)
	{
		auto lines = GetLines(__FILE__);
		auto file = Tools::MappedFile::TryCreate(__FILE__).value();

		ASSERT_LT(0, static_cast<std::int32_t>(lines.size()));
		ASSERT_EQ(lines, file->GetLines());
	}

	//---------------------------------------------------------------------
	TEST(MappedFileTest, EmptyEndLine)
	{
		auto path = CreateFile({ "Test", "\n", "\n" });
		auto expectedLines = GetLines(*path);
		auto file = Tools::MappedFile::TryCreate(*path).value();

		ASSERT_TRUE(file);
		ASSERT_EQ(expectedLines, file->GetLines());
	}
}