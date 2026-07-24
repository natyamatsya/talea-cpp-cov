// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2014 OpenCppCoverage

#pragma once

#include <cstddef>
#include <Windows.h>
#include "TaleaCppCovExport.hpp"

namespace TaleaCppCov
{
	class TALEACPPCOV_DLL Address
	{
	public:
		Address(HANDLE hProcess, void* value);
		Address(const Address&) = default;

		HANDLE GetProcessHandle() const;
		void* GetValue() const;

		bool operator<(const Address& other) const;
		bool operator==(const Address& other) const;
		friend std::wostream& operator<<(std::wostream&, const Address&);

	private:
		Address& operator=(const Address&) = delete;

	private:
		HANDLE hProcess_;
		void* value_;
	};

	// Hash functor so Address can key an unordered_map on the hot path.
	struct TALEACPPCOV_DLL AddressHasher
	{
		std::size_t operator()(const Address&) const noexcept;
	};
}


