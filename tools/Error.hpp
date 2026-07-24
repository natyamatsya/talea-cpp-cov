// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 natyamatsya and talea-cpp-cov contributors

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
	// exception. Carries the file:line-prefixed message the exception used to
	// hold. The former THROW_LAST_ERROR sites format the Win32 error text into
	// this message (via TaleaCppCov::GetErrorMessage) rather than carrying a
	// structured code.
	struct Error
	{
		std::wstring message;
	};

	template <typename T>
	using Expected = std::expected<T, Error>;

	// Throws Tools::ToolsException(error.message). Defined in Error.cpp so
	// callers need not include ToolsException.hpp just to bridge. Because the
	// message was built by MAKE_ERROR with the same file:line prefix the old
	// THROW produced, the thrown exception is byte-for-byte what the caller
	// saw before the migration.
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

// Builds a Tools::Error whose message is "<file>:<line> <message>" — the same
// file:line-prefixed form the old THROW_BASE produced, so a bridged exception
// reads exactly like the thrown one used to.
// Usage: return std::unexpected(MAKE_ERROR(L"Cannot open " << path));
#define MAKE_ERROR(message)                                                    \
	(::Tools::Error{[&] {                                                       \
		std::wostringstream ostr;                                               \
		ostr << ::Tools::GetFilename(__FILE__) << ':' << __LINE__ << ' '        \
		     << message;                                                        \
		return ostr.str();                                                      \
	}()})
