# Vendored Zydis disassembler, amalgamated single-file C distribution.
add_library(Zydis STATIC Zydis.c)
add_library(occ::Zydis ALIAS Zydis)

# Consumers include <Zydis/Zydis.h> relative to the repository root.
target_include_directories(Zydis
	PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}"
	INTERFACE "${CMAKE_SOURCE_DIR}")
target_compile_definitions(Zydis PUBLIC ZYDIS_STATIC_BUILD)
target_compile_options(Zydis PRIVATE
	/W3
	$<$<C_COMPILER_ID:MSVC>:/sdl>)
target_link_libraries(Zydis PRIVATE occ_opt_default)
