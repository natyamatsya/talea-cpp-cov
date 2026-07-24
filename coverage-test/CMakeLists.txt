add_executable(CoverageTest
	CoverageDataTest.cpp
	FileCoverageTest.cpp
	CoverageTest.cpp)

talea_apply_conventions(CoverageTest)
target_link_libraries(CoverageTest PRIVATE talea_gtest talea::Coverage)

target_precompile_headers(CoverageTest PRIVATE pch.h)
talea_register_tests(CoverageTest)
