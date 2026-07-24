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
#include <cstdint>

#include <sstream>
#include <random>
#include <fstream>

#include "coverage/CoverageData.hpp"
#include "coverage/ModuleCoverage.hpp"
#include "coverage/FileCoverage.hpp"
#include "coverage/LineCoverage.hpp"
#include "exporter/binary/CoverageDataSerializer.hpp"
#include "exporter/binary/CoverageDataDeserializer.hpp"

#include "test-helper/TemporaryPath.hpp"
#include "test-helper/CoverageDataComparer.hpp"

using namespace testing;

namespace ExporterTest
{
	namespace
	{					
		//---------------------------------------------------------------------
		void AddRandomFiles(
			Coverage::ModuleCoverage& module,
			std::default_random_engine& generator,
			std::uniform_int_distribution<std::int32_t>& distribution)
		{
			for (std::int32_t fileIndex = 0; fileIndex < 10; ++fileIndex)
			{
				if (distribution(generator))
				{
					auto& file = module.AddFile(std::to_wstring(fileIndex));

					for (std::int32_t line = 0; line < 100; ++line)
					{
						file.AddLine(line, distribution(generator) != 0);

						for (std::uint32_t branchIndex = 0; branchIndex < 2;
						     ++branchIndex)
						{
							if (distribution(generator))
							{
								file.AddBranch(
								    line,
								    Coverage::BranchCoverage{
								        branchIndex,
								        distribution(generator) != 0,
								        distribution(generator) != 0});
							}
						}
					}
				}
			}
		}

		//---------------------------------------------------------------------
		Coverage::CoverageData CreateRandomCoverageData()
		{
			Coverage::CoverageData coverageData{ L"Test\u00E9", 42 };
			std::default_random_engine generator;
			std::uniform_int_distribution<std::int32_t> distribution(0, 1);
			
			for (std::int32_t moduleIndex = 0; moduleIndex < 100; ++moduleIndex)
			{
				if (distribution(generator))
				{
					auto& module = coverageData.AddModule(std::to_wstring(moduleIndex));
					AddRandomFiles(module, generator, distribution);
				}
			}

			coverageData.AddModule("\xE9\xE8\xE0").AddFile("\xE9\xE8\xE0").AddLine(0, true);

			return coverageData;
		}
	}
	
	//-------------------------------------------------------------------------
	TEST(CoverageDataSerializerTest, SerializeAndDeserialize)
	{
		TestHelper::TemporaryPath path;
		Exporter::CoverageDataSerializer serializer;
		auto randomCoverageData = CreateRandomCoverageData();

		serializer.Serialize(randomCoverageData, path.GetPath().string());

		Exporter::CoverageDataDeserializer deserializer;		
		auto coverageDataRestored = deserializer.Deserialize(path.GetPath().string(), "").value();

		TestHelper::CoverageDataComparer().AssertEquals(randomCoverageData, coverageDataRestored);
	}

	//-------------------------------------------------------------------------
	TEST(CoverageDataSerializerTest, InvalidFile)
	{
		Exporter::CoverageDataDeserializer deserializer;
		TestHelper::TemporaryPath path;
		std::ofstream ofs{ path.GetPath().string(), std::ios::binary};

		std::default_random_engine generator;		
		std::uniform_int_distribution<std::int32_t> distribution(0, 1);

		for (std::int32_t i = 0; i < 100; ++i)
			ofs << distribution(generator);

		ASSERT_FALSE(deserializer.Deserialize(path.GetPath(), "todo"));
	}
}