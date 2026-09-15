set(LZ4_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Includes/LZ4")

set(LZ4_SOURCES
    "${LZ4_DIR}/lz4.h"
    "${LZ4_DIR}/lz4.c"
    "${LZ4_DIR}/lz4hc.h"
    "${LZ4_DIR}/lz4hc.c"
    "${LZ4_DIR}/lz4frame.h"
    "${LZ4_DIR}/lz4frame.c"
    "${LZ4_DIR}/xxhash.h"
    "${LZ4_DIR}/xxhash.c"
)

set_source_files_properties(${LZ4_SOURCES} PROPERTIES 
    SKIP_PRECOMPILE_HEADERS ON
    COMPILE_FLAGS "/Y-"
)

add_sources("NoUberFile"
    SOURCE_GROUP "Includes\\\\LZ4"
        ${LZ4_SOURCES}
)