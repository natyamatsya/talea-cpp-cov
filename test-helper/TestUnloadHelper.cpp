// OpenCppCoverage is an open source code coverage for C++.
// Copyright (C) 2016 OpenCppCoverage
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
