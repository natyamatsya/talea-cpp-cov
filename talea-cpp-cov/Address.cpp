// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2014 OpenCppCoverage

#include "stdafx.h"
#include "Address.hpp"

#include <cstdint>
#include <iostream>

namespace TaleaCppCov
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
