// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2014 OpenCppCoverage

#pragma once

#include <exception>
#include <string>
#include <sstream>

#include "ToolsExport.hpp"

#pragma warning(push)
#pragma warning(disable: 4275) // warning C4275: non dll-interface class 'std::exception' used as base for dll-interface class

namespace Tools
{

	class TOOLS_DLL ExceptionBase : public std::exception
	{
	protected:
		ExceptionBase(const std::wstring& message);
	};

	TOOLS_DLL std::wstring GetFilename(const char*);
}

#pragma warning(pop)

#define GENERATE_EXCEPTION_CLASS(namespaceName, exceptionName)	\
namespace namespaceName											\
{																\
	class exceptionName : public Tools::ExceptionBase			\
	{															\
	public:														\
			explicit exceptionName(const std::wstring& message) \
			: ExceptionBase(message)							\
		{														\
		}														\
	};															\
}
