// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2017 OpenCppCoverage

#include "stdafx.h"
#include <cstdint>

#include "talea-cpp-cov/BreakPoint.hpp"
#include <random>

using TaleaCppCov::BreakPoint;

namespace TaleaCppCovTest
{
	namespace
	{
		//-------------------------------------------------------------------------
		std::set<size_t> GetRandomIndexes(std::int32_t count, std::int32_t maxValue)
		{
			std::mt19937 gen;
			std::uniform_int_distribution<> dis(0, maxValue);
			std::set<size_t> indexes;

			while (indexes.size() < static_cast<size_t>(count))
				indexes.insert(dis(gen));
			return indexes;
		}

		//---------------------------------------------------------------------
		std::vector<std::uint8_t> GenerateValues(std::int32_t valueCount,
		                                          std::int32_t moduloValue)
		{
			std::vector<std::uint8_t> values;

			for (auto i = 0; i < valueCount; ++i)
				values.push_back(i % moduloValue);
			return values;
		}

		//---------------------------------------------------------------------
		std::map<DWORD64, std::uint8_t> BuildOldInstructionsMap(
		    BreakPoint::InstructionCollection& oldInstructionCollection,
		    const std::vector<DWORD64>& addresses)
		{
			std::set<DWORD64> addressesSet{addresses.begin(), addresses.end()};
			std::map<DWORD64, std::uint8_t> oldInstructionsMap;

			for (const auto& pair : oldInstructionCollection)
			{
				oldInstructionsMap.emplace(pair.second, pair.first);
				if (addressesSet.erase(pair.second) != 1)
					throw std::runtime_error("Cannot found address");
			}
			if (!addressesSet.empty())
				throw std::runtime_error("Some addresses are not found");
			return oldInstructionsMap;
		}

		//---------------------------------------------------------------------
		template <typename T>
		DWORD64 ToDWORD64(const T* address)
		{
			return reinterpret_cast<DWORD64>(address);
		}
	}

	//-------------------------------------------------------------------------
	TEST(BreakPointTest, SetBreakPoints)
	{
		BreakPoint breakPoint;

		auto values = GenerateValues(20000, 100);
		auto randomIndexes = GetRandomIndexes(100, static_cast<std::int32_t>(values.size() - 1));

		std::vector<DWORD64> addresses;
		for (auto index : randomIndexes)
			addresses.push_back(ToDWORD64(&values[index]));

		auto oldInstructionCollection = Tools::ValueOrThrow(
		    breakPoint.SetBreakPoints(GetCurrentProcess(), std::move(addresses)));
		auto oldInstructionsMap =
		    BuildOldInstructionsMap(oldInstructionCollection, addresses);

		for (size_t i = 0; i < values.size(); ++i)
		{
			auto address = ToDWORD64(&values[i]);
			auto it = oldInstructionsMap.find(address);

			if (it != oldInstructionsMap.end())
			{
				ASSERT_EQ(BreakPoint::breakPointInstruction, values[i]);
				ASSERT_EQ(i % 100, it->second);
			}
			else
				ASSERT_EQ(i % 100, values[i]);
		}
	}

	//-------------------------------------------------------------------------
	TEST(BreakPointTest, SetBreakPointsSingle)
	{
		TaleaCppCov::BreakPoint breakPoint;
		std::uint8_t value = 42;

		auto oldInstructionCollection = Tools::ValueOrThrow(
		    breakPoint.SetBreakPoints(GetCurrentProcess(), {ToDWORD64(&value)}));

		ASSERT_EQ(1, oldInstructionCollection.size());
		ASSERT_EQ(BreakPoint::breakPointInstruction, value);
		ASSERT_EQ(42, oldInstructionCollection.at(0).first);
		ASSERT_EQ(ToDWORD64(&value), oldInstructionCollection.at(0).second);
	}
}