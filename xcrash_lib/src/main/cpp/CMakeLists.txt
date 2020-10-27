cmake_minimum_required(VERSION 3.4.1)

#######################################
# global
#######################################

add_compile_options(
        -std=c11
        -Weverything
        -Werror)

#######################################
# libxcrash.so
#######################################

file(GLOB XCRASH_SRC
        xcrash/*.c
        common/*.c
        dl/*.c)

add_library(xcrash SHARED
        ${XCRASH_SRC})

target_include_directories(xcrash PUBLIC
        xcrash
        xcrash_dumper
        common
        dl)

target_link_libraries(xcrash
        log
        dl)

if(USEASAN)

target_compile_options(xcrash PUBLIC
        -fsanitize=address
        -fno-omit-frame-pointer)

set_target_properties(xcrash PROPERTIES
        LINK_FLAGS " \
        -fsanitize=address")

else()

target_compile_options(xcrash PUBLIC
        -Oz
        -flto
        -ffunction-sections
        -fdata-sections)

set_target_properties(xcrash PROPERTIES
        LINK_FLAGS " \
        -O3 \
        -flto \
        -Wl,--exclude-libs,ALL \
        -Wl,--gc-sections \
        -Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/xcrash.exports")

endif()

#######################################
# libxcrash_dumper.so
#######################################

file(GLOB XCRASH_DUMPER_SRC
        xcrash_dumper/*.c
        common/*.c)

set(LZME_SRC
        lzma/7zCrc.c
        lzma/7zCrcOpt.c
        lzma/CpuArch.c
        lzma/Bra.c
        lzma/Bra86.c
        lzma/BraIA64.c
        lzma/Delta.c
        lzma/Lzma2Dec.c
        lzma/LzmaDec.c
        lzma/Sha256.c
        lzma/Xz.c
        lzma/XzCrc64.c
        lzma/XzCrc64Opt.c
        lzma/XzDec.c)

set_source_files_properties(${LZME_SRC} PROPERTIES
        COMPILE_FLAGS " \
        -D_7ZIP_ST \
        -Wno-enum-conversion \
        -Wno-reserved-id-macro \
        -Wno-undef \
        -Wno-missing-prototypes \
        -Wno-missing-variable-declarations \
        -Wno-cast-align \
        -Wno-sign-conversion \
        -Wno-assign-enum \
        -Wno-unused-macros \
        -Wno-padded \
        -Wno-cast-qual \
        -Wno-strict-prototypes \
        -Wno-extra-semi-stmt")

add_executable(xcrash_dumper
        ${XCRASH_DUMPER_SRC}
        ${LZME_SRC})

target_include_directories(xcrash_dumper PUBLIC
        xcrash_dumper
        common
        lzma)

target_link_libraries(xcrash_dumper
        log
        dl)

if(USEASAN)

target_compile_options(xcrash_dumper PUBLIC
        -fsanitize=address
        -fno-omit-frame-pointer)

set_target_properties(xcrash_dumper PROPERTIES
        LINK_FLAGS " \
        -fsanitize=address")

else()

target_compile_options(xcrash_dumper PUBLIC
        -Oz
        -flto
        -ffunction-sections
        -fdata-sections)

set_target_properties(xcrash_dumper PROPERTIES
        LINK_FLAGS " \
        -O3 \
        -flto \
        -Wl,--exclude-libs,ALL \
        -Wl,--gc-sections")

endif()

set_target_properties(xcrash_dumper PROPERTIES
        PREFIX "lib"
        SUFFIX ".so")
