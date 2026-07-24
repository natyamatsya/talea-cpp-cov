// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2016 OpenCppCoverage

// This translation unit is built on its own into TestHelper.dll, the tiny DLL
// the UnloadReloadDll coverage fixture loads at runtime (LoadLibrary /
// GetProcAddress "TestUnloadDll" / FreeLibrary) to exercise unload-and-reload
// coverage. It is deliberately a C-ABI leaf with no CRT-owning state crossing
// the DLL boundary, so it is safe under the static CRT. TestHelper::
// GetTestUnloadDllFilename() returns this file's name, which is the source the
// coverage tool reports when TestUnloadDll runs - keep the two in sync.
extern "C" __declspec(dllexport) void TestUnloadDll()
{
}
