# TestHelper is a static utility library (comparers, temp paths, process
# launching) linked directly into the test executables. It used to be a DLL,
# but under the static CRT its STL-returning helpers cannot cross a module
# boundary, so it is now compiled into each test exe instead.
add_library(TestHelper STATIC
	CoverageDataComparer.cpp
	TemporaryPath.cpp
	Tools.cpp)
add_library(occ::TestHelper ALIAS TestHelper)

occ_apply_conventions(TestHelper)
target_link_libraries(TestHelper
	PUBLIC occ_gtest occ::Plugin occ::Tools)

target_precompile_headers(TestHelper PRIVATE stdafx.h)

# The tiny DLL the UnloadReloadDll coverage fixture loads at runtime. It is a
# standalone C-ABI leaf (one exported no-op function) built as TestHelper.dll,
# kept separate from the static utility library above.
add_library(TestUnloadHelper SHARED TestUnloadHelper.cpp)
# The DLL must be named TestHelper.dll (the fixture LoadLibrary's it by that
# name), but its import library must not collide with the static TestHelper.lib
# above - the DLL is only ever loaded at runtime, never linked, so the import
# lib name is arbitrary.
set_target_properties(TestUnloadHelper PROPERTIES
	RUNTIME_OUTPUT_NAME TestHelper
	ARCHIVE_OUTPUT_NAME TestUnloadHelper)
occ_apply_conventions(TestUnloadHelper)
