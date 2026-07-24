add_library(Tools STATIC
	Error.cpp
	ExceptionBase.cpp
	Log.cpp
	MappedFile.cpp
	MiniDump.cpp
	PEFileHeader.cpp
	ProcessMemory.cpp
	Regex.cpp
	ScopedAction.cpp
	Tool.cpp
	UniquePath.cpp
	WarningManager.cpp
	dllmain.cpp)
add_library(talea::Tools ALIAS Tools)

talea_apply_conventions(Tools)
target_compile_definitions(Tools PRIVATE TOOLS_EXPORTS)
# fmt/spdlog are PUBLIC because Log.hpp exposes them to every consumer; both
# are linked statically into Tools.dll per the custom vcpkg triplet.
target_link_libraries(Tools
	PUBLIC fmt::fmt spdlog::spdlog
	PRIVATE PCRE2::8BIT mio::mio Dbghelp)

target_precompile_headers(Tools PRIVATE stdafx.h)
set_source_files_properties(dllmain.cpp PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
