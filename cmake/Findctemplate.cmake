# Locates ctemplate as installed by vcpkg (static per custom-triplets; the
# port installs no CMake config, so a find module is required) and provides
# the imported target ctemplate::ctemplate.
#
# Consumers must define CTEMPLATE_DLL_DECL= (empty); talea_defaults does.

find_path(ctemplate_INCLUDE_DIR ctemplate/template.h)
find_library(ctemplate_LIBRARY NAMES libctemplate ctemplate)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(ctemplate
	REQUIRED_VARS ctemplate_LIBRARY ctemplate_INCLUDE_DIR)

if(ctemplate_FOUND AND NOT TARGET ctemplate::ctemplate)
	add_library(ctemplate::ctemplate UNKNOWN IMPORTED)
	set_target_properties(ctemplate::ctemplate PROPERTIES
		IMPORTED_LOCATION "${ctemplate_LIBRARY}"
		INTERFACE_INCLUDE_DIRECTORIES "${ctemplate_INCLUDE_DIR}")
endif()
