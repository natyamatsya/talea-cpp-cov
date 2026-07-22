add_executable(PluginTest
	Exporter/CoverageDataTest.cpp
	Exporter/FileCoverageTest.cpp
	PluginTest.cpp)

occ_apply_conventions(PluginTest)
target_link_libraries(PluginTest PRIVATE occ_gtest occ::Plugin)

target_precompile_headers(PluginTest PRIVATE pch.h)
occ_register_tests(PluginTest)
