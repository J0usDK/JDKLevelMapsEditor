set(ZSTD_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Includes/Zstd")

set(ZSTD_SOURCES
    "${ZSTD_DIR}/zstd.h"
    "${ZSTD_DIR}/zstd_errors.h"
)

set(ZSTD_SOURCES_COMMON
    "${ZSTD_DIR}/common/allocations.h"
    "${ZSTD_DIR}/common/bits.h"
    "${ZSTD_DIR}/common/bitstream.h"
    "${ZSTD_DIR}/common/compiler.h"
    "${ZSTD_DIR}/common/cpu.h"
    "${ZSTD_DIR}/common/debug.h"
    "${ZSTD_DIR}/common/debug.c"
    "${ZSTD_DIR}/common/entropy_common.c"
    "${ZSTD_DIR}/common/error_private.h"
    "${ZSTD_DIR}/common/error_private.c"
    "${ZSTD_DIR}/common/fse.h"
    "${ZSTD_DIR}/common/fse_decompress.c"
    "${ZSTD_DIR}/common/huf.h"
    "${ZSTD_DIR}/common/mem.h"
    "${ZSTD_DIR}/common/pool.h"
    "${ZSTD_DIR}/common/pool.c"
    "${ZSTD_DIR}/common/portability_macros.h"
    "${ZSTD_DIR}/common/threading.h"
    "${ZSTD_DIR}/common/threading.c"
    "${ZSTD_DIR}/common/xxhash.h"
    "${ZSTD_DIR}/common/xxhash.c"
    "${ZSTD_DIR}/common/zstd_common.c"
    "${ZSTD_DIR}/common/zstd_deps.h"
    "${ZSTD_DIR}/common/zstd_internal.h"
    "${ZSTD_DIR}/common/zstd_trace.h"
)

set(ZSTD_SOURCES_COMPRESS
    "${ZSTD_DIR}/compress/clevels.h"
    "${ZSTD_DIR}/compress/fse_compress.c"
    "${ZSTD_DIR}/compress/hist.h"
    "${ZSTD_DIR}/compress/hist.c"
    "${ZSTD_DIR}/compress/huf_compress.c"
    "${ZSTD_DIR}/compress/zstd_compress.c"
    "${ZSTD_DIR}/compress/zstd_compress_internal.h"
    "${ZSTD_DIR}/compress/zstd_compress_literals.c"
    "${ZSTD_DIR}/compress/zstd_compress_literals.h"
    "${ZSTD_DIR}/compress/zstd_compress_sequences.c"
    "${ZSTD_DIR}/compress/zstd_compress_sequences.h"
    "${ZSTD_DIR}/compress/zstd_compress_superblock.c"
    "${ZSTD_DIR}/compress/zstd_compress_superblock.h"
    "${ZSTD_DIR}/compress/zstd_cwksp.h"
    "${ZSTD_DIR}/compress/zstd_double_fast.c"
    "${ZSTD_DIR}/compress/zstd_double_fast.h"
    "${ZSTD_DIR}/compress/zstd_fast.c"
    "${ZSTD_DIR}/compress/zstd_fast.h"
    "${ZSTD_DIR}/compress/zstd_lazy.c"
    "${ZSTD_DIR}/compress/zstd_lazy.h"
    "${ZSTD_DIR}/compress/zstd_ldm.c"
    "${ZSTD_DIR}/compress/zstd_ldm.h"
    "${ZSTD_DIR}/compress/zstd_ldm_geartab.h"
    "${ZSTD_DIR}/compress/zstd_opt.c"
    "${ZSTD_DIR}/compress/zstd_opt.h"
    "${ZSTD_DIR}/compress/zstd_preSplit.c"
    "${ZSTD_DIR}/compress/zstd_preSplit.h"
    "${ZSTD_DIR}/compress/zstdmt_compress.c"
    "${ZSTD_DIR}/compress/zstdmt_compress.h"
)

set(ZSTD_SOURCES_DECOMPRESS
    "${ZSTD_DIR}/decompress/huf_decompress.c"
    "${ZSTD_DIR}/decompress/zstd_ddict.c"
    "${ZSTD_DIR}/decompress/zstd_ddict.h"
    "${ZSTD_DIR}/decompress/zstd_decompress.c"
    "${ZSTD_DIR}/decompress/zstd_decompress_block.c"
    "${ZSTD_DIR}/decompress/zstd_decompress_block.h"
    "${ZSTD_DIR}/decompress/zstd_decompress_internal.h"
)

# Optional architecture-specific assembly implementation.
set(ZSTD_SOURCES_ASM)

# huf_decompress_amd64.S uses GNU-style assembly.
# Do not use it with MSVC, and only use it for x86-64 targets.
if (NOT MSVC AND CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|X86_64|amd64|AMD64).*$")
    include(CheckLanguage)
    check_language(ASM)
    if (CMAKE_ASM_COMPILER)
        enable_language(ASM)
        list(APPEND ZSTD_SOURCES_ASM
            "${ZSTD_DIR}/decompress/huf_decompress_amd64.S"
        )
    else()
        add_compile_definitions(ZSTD_DISABLE_ASM)
    endif()
else()
    add_compile_definitions(ZSTD_DISABLE_ASM)
endif()

set_source_files_properties(
    ${ZSTD_SOURCES}
    ${ZSTD_SOURCES_COMMON}
    ${ZSTD_SOURCES_COMPRESS}
    ${ZSTD_SOURCES_DECOMPRESS}
    PROPERTIES
        SKIP_PRECOMPILE_HEADERS ON
        COMPILE_FLAGS "/Y-"
)

add_sources("NoUberFile"
    SOURCE_GROUP "Includes\\\\Zstd"
        ${ZSTD_SOURCES}
        ${ZSTD_SOURCES_COMMON}
        ${ZSTD_SOURCES_COMPRESS}
        ${ZSTD_SOURCES_DECOMPRESS}
        ${ZSTD_SOURCES_ASM}
)