// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2014 OpenCppCoverage

#pragma once

namespace TestHelper
{
	//---------------------------------------------------------------------
	template<typename Container, typename CompareFct>
	void AssertContainerEqual(
		const Container& container1,
		const Container& container2,
		const CompareFct& compareFct)
	{
		ASSERT_EQ(container1.size(), container2.size());

		for (size_t i = 0; i < container1.size(); ++i)
		{
			const auto& value1 = container1[i];
			const auto& value2 = container2[i];

			compareFct(value1, value2);
		}
	}

	//---------------------------------------------------------------------
	template<typename Container, typename CompareFct>
	void AssertContainerUniquePtrEqual(
		const Container& container1,
		const Container& container2,
		const CompareFct& compareFct)
	{
		using ValueType = typename Container::value_type;
		AssertContainerEqual(container1, container2,
			[&](const ValueType& value1, const ValueType& value2)
		{
			ASSERT_TRUE(static_cast<bool>(value1));
			ASSERT_TRUE(static_cast<bool>(value2));
			compareFct(*value1, *value2);
		});
	}

	//---------------------------------------------------------------------
	template<typename Key, typename Container, typename CompareFct>
	bool IsFirstContainsSecond(
		const Container& container1,
		const Container& container2,
		const std::function<Key (const typename Container::value_type&)>& getKeyFct,
		const CompareFct& compareFct)
	{
		std::map<Key, const typename Container::value_type*> container1ByKey;

		for (const auto& object : container1)
			container1ByKey.emplace(getKeyFct(object), &object);
		for (const auto& object : container2)
		{
			auto it = container1ByKey.find(getKeyFct(object));

			if (it == container1ByKey.end())
				return false;

			return compareFct(*it->second, object);
		}
		return true;
	}
}