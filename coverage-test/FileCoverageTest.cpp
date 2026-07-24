// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2014 OpenCppCoverage

#include "pch.h"

#include <cstdint>

#include "coverage/FileCoverage.hpp"
#include "talea-cpp-cov/TaleaCppCovException.hpp"

namespace cov = TaleaCppCov;

namespace CoverageTest
{
	//-------------------------------------------------------------------------
	TEST(FileCoverageTest, Basic)
	{
		Coverage::FileCoverage file{ L"" };
		std::uint32_t lineNumber = 0;

		file.AddLine(lineNumber, true);

		auto line = file[lineNumber];
		ASSERT_NE(nullptr, line);
		ASSERT_TRUE(line->HasBeenExecuted());
		ASSERT_EQ(nullptr, file[lineNumber + 1]);	

		const auto& lines = file.GetLines();
		ASSERT_EQ(1, lines.size());
		ASSERT_EQ(line->GetLineNumber(), lines[0].GetLineNumber());
	}	

	//-------------------------------------------------------------------------
	TEST(FileCoverageTest, UpdateLine)
	{
		Coverage::FileCoverage file{ L"" };
		std::uint32_t lineNumber = 0;

		file.AddLine(lineNumber, true);
		file.UpdateLine(lineNumber, false);

		ASSERT_FALSE(file.GetLines().at(0).HasBeenExecuted());		
	}

	//-------------------------------------------------------------------------
	TEST(FileCoverageTest, UpdateLineNotExists)
	{
		Coverage::FileCoverage file{ L"" };
		
		ASSERT_THROW(file.UpdateLine(0, false), std::runtime_error);
	}
}