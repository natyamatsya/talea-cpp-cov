add_executable(ToolsTest
	LogFormatTest.cpp
	MappedFileTest.cpp
	ToolTest.cpp
	ToolsTest.cpp)

occ_apply_conventions(ToolsTest)
target_link_libraries(ToolsTest PRIVATE
	occ_boost occ_gtest occ::TestHelper occ::Tools)

target_precompile_headers(ToolsTest PRIVATE stdafx.h)
occ_register_tests(ToolsTest)
