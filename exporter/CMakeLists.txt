add_library(Exporter STATIC
	binary/BinaryExporter.cpp
	binary/ColumnarCoverage.cpp
	binary/CoverageDataDeserializer.cpp
	binary/CoverageDataSerializer.cpp
	CoberturaExporter.cpp
	html/HtmlExporter.cpp
	html/HtmlFile.cpp
	html/HtmlFileCoverageExporter.cpp
	html/HtmlFolderStructure.cpp
	html/TemplateHtmlExporter.cpp
	InvalidOutputFileException.cpp)
add_library(talea::Exporter ALIAS Exporter)

talea_apply_conventions(Exporter)
target_compile_definitions(Exporter PRIVATE
	EXPORTER_EXPORTS
	_SILENCE_CXX17_ITERATOR_BASE_CLASS_DEPRECATION_WARNING)
target_compile_options(Exporter PRIVATE /wd4251)
# ctemplate is PUBLIC: html/TemplateHtmlExporter.hpp exposes it to consumers
# (ExporterTest instantiates ctemplate symbols directly).
target_link_libraries(Exporter
	PUBLIC talea::TaleaCppCov talea::Coverage talea::Tools ctemplate::ctemplate
	PRIVATE fmt::fmt)

target_precompile_headers(Exporter PRIVATE stdafx.h)

# The HTML exporter reads its templates from <exe dir>/Template.
add_custom_command(TARGET Exporter POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy_directory
		"${CMAKE_CURRENT_SOURCE_DIR}/html/template"
		"${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/Template"
	VERBATIM)
