// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 natyamatsya and talea-cpp-cov contributors

#include <cstdint>
#include <cstring>

#include <windows.h>

// Host for TestFastCovDll. It is not instrumented itself but links the
// fast-mode runtime, like a test executable of an instrumented library:
//   TestFastCovDllHost unload - load the DLL, call it, FreeLibrary, exit
//   TestFastCovDllHost keep   - load the DLL, call it, exit with it loaded
// Exit code 0 on success.

int main(int argc, char* argv[])
{
	if (argc != 2)
		return 2;

	auto dll = LoadLibraryW(L"TestFastCovDll.dll");
	if (!dll)
		return 3;
	using ClassifyFn = std::int32_t (*)(std::int32_t);
	auto classify = reinterpret_cast<ClassifyFn>(
	    GetProcAddress(dll, "TestFastCovDllClassify"));
	if (!classify || classify(argc) <= 0)
		return 4;

	if (std::strcmp(argv[1], "unload") == 0)
		return FreeLibrary(dll) ? 0 : 5;
	return std::strcmp(argv[1], "keep") == 0 ? 0 : 6;
}
