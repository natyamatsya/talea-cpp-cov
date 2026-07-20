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
#include "Address.hpp"

#include <cstdint>
#include <iostream>

namespace CppCoverage
{			
	//-------------------------------------------------------------------------
	Address::Address(HANDLE hProcess, void* value)
	: hProcess_{ hProcess }
	, value_{ value }
	{
	}

	//-------------------------------------------------------------------------
	HANDLE Address::GetProcessHandle() const
	{
		return hProcess_;
	}

	//-------------------------------------------------------------------------
	void* Address::GetValue() const
	{
		return value_;
	}

	//-------------------------------------------------------------------------
	bool Address::operator<(const Address& other) const
	{
		if (hProcess_ != other.hProcess_)
			return hProcess_ < other.hProcess_;

		return value_ < other.value_;
	}

	//-------------------------------------------------------------------------
	bool Address::operator==(const Address& other) const
	{
		return hProcess_ == other.hProcess_ && value_ == other.value_;
	}

	//-------------------------------------------------------------------------
	std::wostream& operator<<(std::wostream& ostr, const Address& address)
	{
		return ostr << '[' << address.hProcess_ << "]: " << address.value_;
	}

	//-------------------------------------------------------------------------
	std::size_t AddressHasher::operator()(const Address& address) const noexcept
	{
		auto value = reinterpret_cast<std::uintptr_t>(address.GetValue());
		auto process =
		    reinterpret_cast<std::uintptr_t>(address.GetProcessHandle());

		// Mix the address value (which varies per breakpoint) with the
		// process handle (shared across a process's breakpoints).
		std::size_t seed = static_cast<std::size_t>(value);
		seed ^= static_cast<std::size_t>(process) + 0x9E3779B97F4A7C15ULL +
		        (seed << 6) + (seed >> 2);
		return seed;
	}
}
