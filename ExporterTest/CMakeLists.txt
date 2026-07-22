# Data/TestFile1.cpp and Data/TestFile2.cpp are fixture inputs, not sources.
add_executable(ExporterTest
	BinaryExporterTest.cpp
	CoberturaExporterTest.cpp
	ColumnarCoverageTest.cpp
	CoverageDataSerializerTest.cpp
	ExporterTest.cpp
	HtmlExporterTest.cpp
	HtmlFileCoverageExporterTest.cpp
	HtmlFolderStructureTest.cpp
	TemplateHtmlExporterTest.cpp)

occ_apply_conventions(ExporterTest)
target_compile_definitions(ExporterTest PRIVATE
	$<$<CONFIG:Debug>:_SCL_SECURE_NO_WARNINGS>)
target_link_libraries(ExporterTest PRIVATE
	occ_gtest
	occ::CppCoverage occ::Exporter occ::Plugin occ::TestHelper occ::Tools)

target_precompile_headers(ExporterTest PRIVATE stdafx.h)
# Writes report directories at fixed paths.
occ_register_tests(ExporterTest SERIAL TIMEOUT 600)
