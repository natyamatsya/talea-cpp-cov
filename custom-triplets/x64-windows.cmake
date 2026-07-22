# Overlay of the built-in x64-windows triplet.
#
# Everything stays dynamic (DLL + dynamic CRT) as upstream, EXCEPT a handful of
# "leaf" libraries that are each used inside a single one of our DLLs and carry
# no cross-module singleton state. Those are linked statically so we do not have
# to ship their DLLs (pcre2-8.dll, fmt.dll, libctemplate.dll, benchmark.dll):
#
#   pcre2      -> folded into Tools.dll     (Tools::Regex)
#   spdlog     -> folded into Tools.dll     (logging backend, only in Log.cpp)
#   fmt        -> folded into Tools.dll + Exporter.dll (spdlog / CoberturaExporter)
#   ctemplate  -> folded into Exporter.dll  (HTML export)
#   benchmark  -> folded into Benchmarks.exe
#
# The logging backend (spdlog) is confined to Tools/Log.cpp so a single logger
# instance lives in Tools.dll and can link statically — unlike boost-log, whose
# LOG_* macros expanded inline in every module and required the shared DLL.
set(VCPKG_TARGET_ARCHITECTURE x64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE dynamic)

if(PORT MATCHES "^(pcre2|fmt|ctemplate|benchmark|spdlog)$")
    set(VCPKG_LIBRARY_LINKAGE static)
endif()
