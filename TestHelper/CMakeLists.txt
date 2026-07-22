add_library(TestHelper SHARED
	CoverageDataComparer.cpp
	TemporaryPath.cpp
	Tools.cpp)
add_library(occ::TestHelper ALIAS TestHelper)

occ_apply_conventions(TestHelper)
target_compile_definitions(TestHelper PRIVATE TEST_HELPER_EXPORTS)
target_link_libraries(TestHelper
	PUBLIC occ_gtest occ::Plugin occ::Tools)

target_precompile_headers(TestHelper PRIVATE stdafx.h)
