// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2014 OpenCppCoverage

#pragma once
#include <cstdint>

#include "tools/ExceptionBase.hpp"
#include "TaleaCppCovExport.hpp"
#include <string>

namespace TaleaCppCov
{
	TALEACPPCOV_DLL std::wstring GetErrorMessage(std::int32_t errorCode);
}


GENERATE_EXCEPTION_CLASS(TaleaCppCov, TaleaCppCovException);
