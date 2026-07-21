# Overlay of the built-in x64-windows triplet.
#
# Everything stays dynamic (DLL + dynamic CRT) as upstream, EXCEPT a handful of
# "leaf" libraries that are each used inside a single one of our DLLs and carry
# no cross-module singleton state. Those are linked statically so we do not have
# to ship their DLLs (pcre2-8.dll, fmt.dll, libctemplate.dll, benchmark.dll):
#
#   pcre2      -> folded into Tools.dll     (Tools::Regex)
#   fmt        -> folded into Exporter.dll  (CoberturaExporter)
#   ctemplate  -> folded into Exporter.dll  (HTML export)
#   benchmark  -> folded into Benchmarks.exe
#
# boost-log deliberately stays dynamic: the LOG_* macros expand inline in every
# module to a boost-log global-logger singleton, which is only shared across our
# DLLs when boost-log itself is the shared DLL.
set(VCPKG_TARGET_ARCHITECTURE x64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE dynamic)

if(PORT MATCHES "^(pcre2|fmt|ctemplate|benchmark)$")
    set(VCPKG_LIBRARY_LINKAGE static)
endif()
