SET(xcrash_sources
        xc_common.c
        xc_crash.c
        xc_dl.c
        xc_fallback.c
        xc_jni.c
        xc_test.c
        xc_trace.c
        xc_util.c)
ADD_LIBRARY(xcrash SHARED ${xcrash_sources})
TARGET_INCLUDE_DIRECTORIES(xcrash PRIVATE .)
TARGET_COMPILE_OPTIONS(xcrash
        PRIVATE
        -std=c11
        -Weverything
        -Werror
        -fvisibility=hidden)
TARGET_LINK_LIBRARIES(xcrash xcrash_common ${libdl} ${liblog})
