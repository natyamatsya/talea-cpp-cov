// TaleaCppCovConsole.cpp : Defines the entry point for the console application.
//

// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2014 OpenCppCoverage

#include "stdafx.h"

// When built with -DOCC_MIMALLOC=ON, override global new/delete with mimalloc
// for the whole executable. Included in the entry-point TU so the overrides are
// visible program-wide; a no-op when the option is off.
#ifdef TALEA_USE_MIMALLOC
#include <mimalloc-new-delete.h>
#endif

#include <iostream>

#include "tools/MiniDump.hpp"

#include "talea-cpp-cov.hpp"

//-----------------------------------------------------------------------------
int main(int argc, const char* argv[])
{
	Tools::CreateMiniDumpOnUnHandledException();
	
	try
	{
		TaleaCppCovApp::TaleaCppCovApp taleaCppCovApp;

		return taleaCppCovApp.Run(argc, argv, &std::wcerr);
	}
	catch (const std::exception& e)
	{
		std::cerr << "Error: " << e.what() << std::endl;
	}
	catch (...)
	{
		std::cerr << "Unknown error" << std::endl;
	}

	return TaleaCppCovApp::FailureExitCode;
}