add_executable(ToolsTest
	LogFormatTest.cpp
	MappedFileTest.cpp
	ToolTest.cpp
	ToolsTest.cpp)

talea_apply_conventions(ToolsTest)
target_link_libraries(ToolsTest PRIVATE
	talea_gtest talea::TestHelper talea::Tools)

target_precompile_headers(ToolsTest PRIVATE stdafx.h)
talea_register_tests(ToolsTest)
