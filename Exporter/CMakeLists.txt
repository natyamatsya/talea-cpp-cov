add_library(Exporter STATIC
	Binary/BinaryExporter.cpp
	Binary/ColumnarCoverage.cpp
	Binary/CoverageDataDeserializer.cpp
	Binary/CoverageDataSerializer.cpp
	CoberturaExporter.cpp
	Html/HtmlExporter.cpp
	Html/HtmlFile.cpp
	Html/HtmlFileCoverageExporter.cpp
	Html/HtmlFolderStructure.cpp
	Html/TemplateHtmlExporter.cpp
	InvalidOutputFileException.cpp)
add_library(occ::Exporter ALIAS Exporter)

occ_apply_conventions(Exporter)
target_compile_definitions(Exporter PRIVATE
	EXPORTER_EXPORTS
	_SILENCE_CXX17_ITERATOR_BASE_CLASS_DEPRECATION_WARNING)
target_compile_options(Exporter PRIVATE /wd4251)
# ctemplate is PUBLIC: Html/TemplateHtmlExporter.hpp exposes it to consumers
# (ExporterTest instantiates ctemplate symbols directly).
target_link_libraries(Exporter
	PUBLIC occ::CppCoverage occ::Coverage occ::Tools ctemplate::ctemplate
	PRIVATE fmt::fmt)

target_precompile_headers(Exporter PRIVATE stdafx.h)

# The HTML exporter reads its templates from <exe dir>/Template.
add_custom_command(TARGET Exporter POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy_directory
		"${CMAKE_CURRENT_SOURCE_DIR}/Html/Template"
		"${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/Template"
	VERBATIM)
