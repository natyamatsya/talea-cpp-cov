add_executable(CoverageTest
	CoverageDataTest.cpp
	FileCoverageTest.cpp
	CoverageTest.cpp)

occ_apply_conventions(CoverageTest)
target_link_libraries(CoverageTest PRIVATE occ_gtest occ::Coverage)

target_precompile_headers(CoverageTest PRIVATE pch.h)
occ_register_tests(CoverageTest)
