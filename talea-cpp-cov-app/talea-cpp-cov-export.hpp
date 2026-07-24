// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2014 OpenCppCoverage

#pragma once

#ifdef TALEA_CPP_COV_APP_EXPORTS
#define TALEA_CPP_COV_APP_DLL __declspec(dllexport)
#else
#define TALEA_CPP_COV_APP_DLL _declspec(dllimport)
#endif

#pragma warning (disable: 4251)