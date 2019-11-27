ADD_SUBDIRECTORY(lzma)

SET(xcrash_dumper_sources
        xcd_arm_exidx.c
        xcd_core.c
        xcd_dwarf.c
        xcd_elf.c
        xcd_elf_interface.c
        xcd_frames.c
        xcd_map.c
        xcd_maps.c
        xcd_md5.c
        xcd_memory.c
        xcd_memory_buf.c
        xcd_memory_file.c
        xcd_memory_remote.c
        xcd_process.c
        xcd_regs_arm.c
        xcd_regs_arm64.c
        xcd_regs_x86.c
        xcd_regs_x86_64.c
        xcd_sys.c
        xcd_thread.c
        xcd_util.c)
ADD_LIBRARY(xcrash_dumper SHARED ${xcrash_dumper_sources})
TARGET_INCLUDE_DIRECTORIES(xcrash_dumper PRIVATE .)
TARGET_COMPILE_OPTIONS(xcrash_dumper
        PRIVATE
        -std=c11
        -Weverything
        -Werror
        -fvisibility=hidden)
TARGET_LINK_LIBRARIES(xcrash_dumper PRIVATE xcrash_common lzma ${android-dl} ${android-log})