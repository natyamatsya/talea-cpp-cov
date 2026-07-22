# Locates the Debug Interface Access (DIA) SDK shipped with Visual Studio and
# provides the imported target DiaSDK::DiaSDK (diaguids.lib + headers).
#
# Resolution order: the VSINSTALLDIR environment variable (set by a Native
# Tools prompt), then vswhere.

set(_diasdk_hints)
if(DEFINED ENV{VSINSTALLDIR})
	list(APPEND _diasdk_hints "$ENV{VSINSTALLDIR}/DIA SDK")
endif()

if(NOT _diasdk_hints)
	set(_vswhere "$ENV{ProgramFiles\(x86\)}/Microsoft Visual Studio/Installer/vswhere.exe")
	if(EXISTS "${_vswhere}")
		execute_process(
			COMMAND "${_vswhere}" -latest -products * -property installationPath
			OUTPUT_VARIABLE _vsroot
			OUTPUT_STRIP_TRAILING_WHITESPACE
			ERROR_QUIET)
		if(_vsroot)
			list(APPEND _diasdk_hints "${_vsroot}/DIA SDK")
		endif()
	endif()
	unset(_vswhere)
endif()

find_path(DiaSDK_INCLUDE_DIR dia2.h
	HINTS ${_diasdk_hints}
	PATH_SUFFIXES include)
find_library(DiaSDK_LIBRARY diaguids
	HINTS ${_diasdk_hints}
	PATH_SUFFIXES lib/amd64)
unset(_diasdk_hints)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(DiaSDK
	REQUIRED_VARS DiaSDK_LIBRARY DiaSDK_INCLUDE_DIR)

if(DiaSDK_FOUND AND NOT TARGET DiaSDK::DiaSDK)
	add_library(DiaSDK::DiaSDK UNKNOWN IMPORTED)
	set_target_properties(DiaSDK::DiaSDK PROPERTIES
		IMPORTED_LOCATION "${DiaSDK_LIBRARY}"
		INTERFACE_INCLUDE_DIRECTORIES "${DiaSDK_INCLUDE_DIR}")
endif()
