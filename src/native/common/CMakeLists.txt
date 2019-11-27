SET(xcrash_common_sources
        xcc_b64.c
        xcc_fmt.c
        xcc_libc_support.c
        xcc_meminfo.c
        xcc_signal.c
        xcc_unwind.c
        xcc_unwind_clang.c
        xcc_unwind_libcorkscrew.c
        xcc_unwind_libunwind.c
        xcc_util.c)
ADD_LIBRARY(xcrash_common STATIC ${xcrash_common_sources})
TARGET_INCLUDE_DIRECTORIES(xcrash_common PUBLIC .)
TARGET_COMPILE_OPTIONS(xcrash_common
        PRIVATE
        -std=c11
        -Weverything
        -Werror
        -O0)
