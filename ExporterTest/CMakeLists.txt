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

talea_apply_conventions(ExporterTest)
target_compile_definitions(ExporterTest PRIVATE
	$<$<CONFIG:Debug>:_SCL_SECURE_NO_WARNINGS>)
target_link_libraries(ExporterTest PRIVATE
	talea_gtest
	talea::TaleaCppCov talea::Exporter talea::Coverage talea::TestHelper talea::Tools)

target_precompile_headers(ExporterTest PRIVATE stdafx.h)
# Writes report directories at fixed paths.
talea_register_tests(ExporterTest SERIAL TIMEOUT 600)
