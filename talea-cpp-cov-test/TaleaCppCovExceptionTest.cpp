// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2014 OpenCppCoverage

#include "stdafx.h"

#include "talea-cpp-cov/TaleaCppCovException.hpp"

namespace cov = TaleaCppCov;

namespace TaleaCppCovTest
{
	
	//-------------------------------------------------------------------------
	TEST(TaleaCppCovException, GetErrorMessage)
	{
		ASSERT_NE(L"", cov::GetErrorMessage(1));
	}

	//-------------------------------------------------------------------------
	TEST(TaleaCppCovException, GetErrorMessageInvalid)
	{
		ASSERT_NE(L"", cov::GetErrorMessage(-1));
	}
}