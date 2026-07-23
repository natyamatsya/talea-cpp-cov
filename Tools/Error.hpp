// OpenCppCoverage is an open source code coverage for C++.
// Copyright (C) 2026 OpenCppCoverage
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

#pragma once

#include <expected>
#include <sstream>
#include <string>
#include <utility>

#include "ToolsExport.hpp"
#include "ExceptionBase.hpp" // Tools::GetFilename

namespace Tools
{
	// The uniform error type carried by std::expected in place of a thrown
	// exception. Carries the file:line-prefixed message that ExceptionBase used
	// to hold. (A structured Win32 code can be added here once a THROW_LAST_ERROR
	// site is migrated; none is yet, so it would be dead.)
	struct Error
	{
		std::wstring message;
	};

	template <typename T>
	using Expected = std::expected<T, Error>;

	// Throws Tools::ToolsException(error.message). Defined in Error.cpp so
	// callers need not include ToolsException.hpp (which would redefine the
	// per-module THROW macro). Because the message was built by MAKE_ERROR with
	// the same file:line prefix the old THROW used, the thrown exception is
	// byte-for-byte what the caller saw before.
	[[noreturn]] TOOLS_DLL void ThrowError(const Error& error);

	// Builds an Error from a caught exception's message. The catch->expected
	// bridge for entry points whose internals still throw (their catch clause
	// hands the exception here).
	[[nodiscard]] TOOLS_DLL Error ErrorFromException(const std::exception& e);

	// expected -> throw bridge for callers not yet migrated: unwrap the value or
	// rethrow the error as the Tools::ToolsException the leaf used to throw.
	template <typename T>
	T ValueOrThrow(Expected<T>&& result)
	{
		if (!result)
			ThrowError(result.error());
		return std::move(*result);
	}

	inline void ThrowOnError(const Expected<void>& result)
	{
		if (!result)
			ThrowError(result.error());
	}
}

// Builds a Tools::Error whose message is "<file>:<line> <message>", mirroring
// THROW_BASE so a bridged exception reads exactly like the old thrown one.
// Usage: return std::unexpected(MAKE_ERROR(L"Cannot open " << path));
#define MAKE_ERROR(message)                                                    \
	(::Tools::Error{[&] {                                                       \
		std::wostringstream ostr;                                               \
		ostr << ::Tools::GetFilename(__FILE__) << ':' << __LINE__ << ' '        \
		     << message;                                                        \
		return ostr.str();                                                      \
	}()})
