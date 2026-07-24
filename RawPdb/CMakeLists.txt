# Vendored RawPDB (Molecular Matters). Deliberately not on the project-wide
# conventions: C++17, no /WX, no global defines.
add_library(RawPdb STATIC
	PDB.cpp
	PDB_CoalescedMSFStream.cpp
	PDB_DBIStream.cpp
	PDB_DBITypes.cpp
	PDB_DirectMSFStream.cpp
	PDB_GlobalSymbolStream.cpp
	PDB_IPIStream.cpp
	PDB_ImageSectionStream.cpp
	PDB_InfoStream.cpp
	PDB_ModuleInfoStream.cpp
	PDB_ModuleLineStream.cpp
	PDB_ModuleSymbolStream.cpp
	PDB_NamesStream.cpp
	PDB_PCH.cpp
	PDB_PublicSymbolStream.cpp
	PDB_RawFile.cpp
	PDB_SectionContributionStream.cpp
	PDB_SourceFileStream.cpp
	PDB_TPIStream.cpp
	PDB_Types.cpp)
add_library(talea::RawPdb ALIAS RawPdb)

set_target_properties(RawPdb PROPERTIES
	CXX_STANDARD 17
	CXX_STANDARD_REQUIRED ON)
target_include_directories(RawPdb
	PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}"
	INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_options(RawPdb PRIVATE /W3 $<$<CXX_COMPILER_ID:MSVC>:/sdl>)
target_link_libraries(RawPdb PRIVATE talea_opt_default)
