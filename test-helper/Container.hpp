// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2014 OpenCppCoverage

#pragma once

namespace TestHelper
{
	template<typename Container, typename CompareFct>
	void AssertContainerEqual(
		const Container& container1,
		const Container& container2,
		const CompareFct& compareFct);
	
	template<typename Container, typename CompareFct>
	void AssertContainerUniquePtrEqual(
		const Container& container1,
		const Container& container2,
		const CompareFct& compareFct);

	template<typename Key, typename Container, typename CompareFct>
	bool IsFirstContainsSecond(
		const Container& container1,
		const Container& container2,
		const std::function<Key(const typename Container::value_type&)>& getKeyFct,
		const CompareFct& compareFct);
}

#include "Container.inl"