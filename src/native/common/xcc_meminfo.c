// Copyright (c) 2019-present, iQIYI, Inc. All rights reserved.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//

// Created by caikelun on 2019-03-22.

#include <stdint.h>
#include <sys/types.h>
#include <inttypes.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include "xcc_util.h"
#include "xcc_libc_support.h"
#include "xcc_meminfo.h"

#define XCC_MEMINFO_DELETE_STR     " (deleted)"
#define XCC_MEMINFO_DELETE_STR_LEN 10
#define XCC_MEMINFO_HEAD_FMT       "%13s %8s %8s %8s %8s %8s %8s %8s\n"
#define XCC_MEMINFO_DATA_FMT       "%13s %8zu %8zu %8zu %8zu %8zu %8zu %8zu\n"
#define XCC_MEMINFO_SUM_HEAD_FMT   "%21s %8s\n"
#define XCC_MEMINFO_SUM_DATA_FMT   "%21s %8zu\n"
#define XCC_MEMINFO_SUM_DATA2_FMT  "%21s %8zu %21s %8zu\n"

typedef struct
{
    size_t pss;
    size_t swappable_pss;
    size_t private_dirty;
    size_t shared_dirty;
    size_t private_clean;
    size_t shared_clean;
    size_t swapped_out;
    size_t swapped_out_pss;
} xcc_meminfo_t;

static const char *xcc_meminfo_label[] =
{
    "Native Heap",
    "Dalvik Heap",
    "Dalvik Other",
    "Stack",
    "Cursor",
    "Ashmem",
    "Gfx dev",
    "Other dev",
    ".so mmap",
    ".jar mmap",
    ".apk mmap",
    ".ttf mmap",
    ".dex mmap",
    ".oat mmap",
    ".art mmap",
    "Other mmap",
    //"EGL mtrack",
    //"GL mtrack",
    //"Other mtrack",
    "Unknown",

    ".Heap",
    ".LOS",
    ".Zygote",
    ".NonMoving",

    ".LinearAlloc",
    ".GC",
    ".JITCache",
    ".CompilerMetadata",
    ".IndirectRef",

    ".Boot vdex",
    ".App dex",
    ".App vdex",

    ".App art",
    ".Boot art"
};

enum {
    HEAP_NATIVE = 0,
    HEAP_DALVIK,
    HEAP_DALVIK_OTHER,
    HEAP_STACK,
    HEAP_CURSOR,
    HEAP_ASHMEM,
    HEAP_GL_DEV,
    HEAP_UNKNOWN_DEV,
    HEAP_SO,
    HEAP_JAR,
    HEAP_APK,
    HEAP_TTF,
    HEAP_DEX,
    HEAP_OAT,
    HEAP_ART,
    HEAP_UNKNOWN_MAP,
    //HEAP_GRAPHICS,
    //HEAP_GL,
    //HEAP_OTHER_MEMTRACK,
    HEAP_UNKNOWN,

    // Dalvik extra sections (heap).
    HEAP_DALVIK_NORMAL,
    HEAP_DALVIK_LARGE,
    HEAP_DALVIK_ZYGOTE,
    HEAP_DALVIK_NON_MOVING,

    // Dalvik other extra sections.
    HEAP_DALVIK_OTHER_LINEARALLOC,
    HEAP_DALVIK_OTHER_ACCOUNTING,
    HEAP_DALVIK_OTHER_CODE_CACHE,
    HEAP_DALVIK_OTHER_COMPILER_METADATA,
    HEAP_DALVIK_OTHER_INDIRECT_REFERENCE_TABLE,

    // Boot vdex / app dex / app vdex
    HEAP_DEX_BOOT_VDEX,
    HEAP_DEX_APP_DEX,
    HEAP_DEX_APP_VDEX,

    // App art, boot art.
    HEAP_ART_APP,
    HEAP_ART_BOOT,
    
    _NUM_HEAP,
    _NUM_EXCLUSIVE_HEAP = HEAP_UNKNOWN + 1
};

static void xcc_meminfo_load(FILE *fp, xcc_meminfo_t *stats, int *found_swap_pss)
{
    char       line[1024];
    size_t     len;
    size_t     temp;
    
    uintptr_t  start = 0, end = 0, prev_end = 0;
    char      *name;
    size_t     name_len, name_pos;
    int        pos = 0;

    size_t     pss;
    size_t     swappable_pss;
    size_t     private_dirty;
    size_t     shared_dirty;
    size_t     private_clean;
    size_t     shared_clean;
    size_t     swapped_out;
    size_t     swapped_out_pss;
    float      sharing_proportion = 0.0;

    int        which_heap = HEAP_UNKNOWN;
    int        sub_heap = HEAP_UNKNOWN;
    int        prev_heap = HEAP_UNKNOWN;

    int        skip = 0;
    int        done = 0;
    int        is_swappable = 0;

    if(NULL == fgets(line, sizeof(line), fp)) return;
    
    while(!done)
    {
        prev_heap = which_heap;
        prev_end = end;
        which_heap = HEAP_UNKNOWN;
        sub_heap = HEAP_UNKNOWN;
        skip = 0;
        is_swappable = 0;

        len = strlen(line);
        if (len < 1) return;
        line[--len] = '\0';
        
        if(sscanf(line, "%"SCNxPTR"-%"SCNxPTR" %*s %*x %*x:%*x %*d%n", &start, &end, &pos) != 2)
        {
            skip = 1;
        }
        else
        {
            name_pos = (size_t)pos;
            
            //get name and name length
            while(isspace(line[name_pos]))
                name_pos += 1;
            name = line + name_pos;
            name_len = strlen(name);
            
            //trim the end of the line if it is " (deleted)"
            if(name_len > XCC_MEMINFO_DELETE_STR_LEN &&
               0 == strcmp(name + name_len - XCC_MEMINFO_DELETE_STR_LEN, XCC_MEMINFO_DELETE_STR))
            {
                name_len -= XCC_MEMINFO_DELETE_STR_LEN;
                name[name_len] = '\0';
            }

            if(0 == strncmp(name, "[heap]", 6) ||
               0 == strncmp(name, "[anon:libc_malloc]", 18))
            {
                which_heap = HEAP_NATIVE;
            }
            else if(0 == strncmp(name, "[stack", 6))
            {
                which_heap = HEAP_STACK;
            }
            else if(name_len > 3 && 0 == strcmp(name + name_len - 3, ".so"))
            {
                which_heap = HEAP_SO;
                is_swappable = 1;
            }
            else if(name_len > 4 && 0 == strcmp(name + name_len - 4, ".jar"))
            {
                which_heap = HEAP_JAR;
                is_swappable = 1;
            }
            else if(name_len > 4 && 0 == strcmp(name + name_len - 4, ".apk"))
            {
                which_heap = HEAP_APK;
                is_swappable = 1;
            }
            else if(name_len > 4 && 0 == strcmp(name + name_len - 4, ".ttf"))
            {
                which_heap = HEAP_TTF;
                is_swappable = 1;
            }
            else if((name_len > 4 && NULL != strstr(name, ".dex")) ||
                    (name_len > 5 && 0 == strcmp(name + name_len - 5, ".odex")))
            {
                which_heap = HEAP_DEX;
                sub_heap = HEAP_DEX_APP_DEX;
                is_swappable = 1;
            }
            else if(name_len > 5 && 0 == strcmp(name + name_len - 5, ".vdex"))
            {
                which_heap = HEAP_DEX;
                if(NULL != strstr(name, "@boot") || NULL != strstr(name, "/boot"))
                {
                    sub_heap = HEAP_DEX_BOOT_VDEX;
                }
                else
                {
                    sub_heap = HEAP_DEX_APP_VDEX;
                }
                is_swappable = 1;
            }
            else if(name_len > 4 && 0 == strcmp(name + name_len - 4, ".oat"))
            {
                which_heap = HEAP_OAT;
                is_swappable = 1;
            }
            else if(name_len > 4 && 0 == strcmp(name + name_len - 4, ".art"))
            {
                which_heap = HEAP_ART;
                if(NULL != strstr(name, "@boot") || NULL != strstr(name, "/boot"))
                {
                    sub_heap = HEAP_ART_BOOT;
                }
                else
                {
                    sub_heap = HEAP_ART_APP;
                }
                is_swappable = 1;
            }
            else if(0 == strncmp(name, "/dev/", 5))
            {
                if(0 == strncmp(name, "/dev/kgsl-3d0", 13))
                {
                    which_heap = HEAP_GL_DEV;
                }
                else if(0 == strncmp(name, "/dev/ashmem", 11))
                {
                    if(0 == strncmp(name, "/dev/ashmem/dalvik-", 19))
                    {
                        if(0 == strncmp(name, "/dev/ashmem/dalvik-LinearAlloc", 30))
                        {
                            which_heap = HEAP_DALVIK_OTHER;
                            sub_heap = HEAP_DALVIK_OTHER_LINEARALLOC;
                        }
                        else if(0 == strncmp(name, "/dev/ashmem/dalvik-alloc space", 30) ||
                                0 == strncmp(name, "/dev/ashmem/dalvik-main space", 29))
                        {
                            which_heap = HEAP_DALVIK;
                            sub_heap = HEAP_DALVIK_NORMAL;
                        }
                        else if(0 == strncmp(name, "/dev/ashmem/dalvik-large object space", 37) ||
                                0 == strncmp(name, "/dev/ashmem/dalvik-free list large object space", 47))
                        {
                            which_heap = HEAP_DALVIK;
                            sub_heap = HEAP_DALVIK_LARGE;
                        }
                        else if(0 == strncmp(name, "/dev/ashmem/dalvik-non moving space", 35))
                        {
                            which_heap = HEAP_DALVIK;
                            sub_heap = HEAP_DALVIK_NON_MOVING;
                        }
                        else if(0 == strncmp(name, "/dev/ashmem/dalvik-zygote space", 31))
                        {
                            which_heap = HEAP_DALVIK;
                            sub_heap = HEAP_DALVIK_ZYGOTE;
                        }
                        else if(0 == strncmp(name, "/dev/ashmem/dalvik-indirect ref", 31))
                        {
                            which_heap = HEAP_DALVIK_OTHER;
                            sub_heap = HEAP_DALVIK_OTHER_INDIRECT_REFERENCE_TABLE;
                        }
                        else if(0 == strncmp(name, "/dev/ashmem/dalvik-jit-code-cache", 33) ||
                                0 == strncmp(name, "/dev/ashmem/dalvik-data-code-cache", 34))
                        {
                            which_heap = HEAP_DALVIK_OTHER;
                            sub_heap = HEAP_DALVIK_OTHER_CODE_CACHE;
                        }
                        else if(0 == strncmp(name, "/dev/ashmem/dalvik-CompilerMetadata", 35))
                        {
                            which_heap = HEAP_DALVIK_OTHER;
                            sub_heap = HEAP_DALVIK_OTHER_COMPILER_METADATA;
                        }
                        else
                        {
                            which_heap = HEAP_DALVIK_OTHER;
                            sub_heap = HEAP_DALVIK_OTHER_ACCOUNTING;
                        }
                    }
                    else if(0 == strncmp(name, "/dev/ashmem/CursorWindow", 24))
                    {
                        which_heap = HEAP_CURSOR;
                    }
                    else if(0 == strncmp(name, "/dev/ashmem/libc malloc", 23))
                    {
                        which_heap = HEAP_NATIVE;
                    }
                    else
                    {
                        which_heap = HEAP_ASHMEM;
                    }
                }
                else
                {
                    which_heap = HEAP_UNKNOWN_DEV;
                }
            }
            else if(0 == strncmp(name, "[anon:", 6))
            {
                which_heap = HEAP_UNKNOWN;
            }
            else if(name_len > 0)
            {
                which_heap = HEAP_UNKNOWN_MAP;
            }
            else if(start == prev_end && prev_heap == HEAP_SO)
            {
                which_heap = HEAP_SO; //bss section of a shared library
            }
        }

        pss = 0;
        shared_clean = 0;
        shared_dirty = 0;
        private_clean = 0;
        private_dirty = 0;
        swapped_out = 0;
        swapped_out_pss = 0;

        while(1)
        {
            if(NULL == fgets(line, sizeof(line), fp))
            {
                done = 1;
                break;
            }

            if(line[0] == 'P' && sscanf(line, "Pss: %zu kB", &temp) == 1)
                pss = temp;
            else if(line[0] == 'S' && sscanf(line, "Shared_Clean: %zu kB", &temp) == 1)
                shared_clean = temp;
            else if (line[0] == 'S' && sscanf(line, "Shared_Dirty: %zu kB", &temp) == 1)
                shared_dirty = temp;
            else if (line[0] == 'P' && sscanf(line, "Private_Clean: %zu kB", &temp) == 1)
                private_clean = temp;
            else if (line[0] == 'P' && sscanf(line, "Private_Dirty: %zu kB", &temp) == 1)
                private_dirty = temp;
            else if (line[0] == 'S' && sscanf(line, "Swap: %zu kB", &temp) == 1)
                swapped_out = temp;
            else if (line[0] == 'S' && sscanf(line, "SwapPss: %zu kB", &temp) == 1)
            {
                *found_swap_pss = 1;
                swapped_out_pss = temp;
            }
            else if(sscanf(line, "%"SCNxPTR"-%"SCNxPTR" %*s %*x %*x:%*x %*d", &start, &end) == 2)
                break; // looks like a new mapping
        }

        if(!skip)
        {
            //get swappable_pss
            if(is_swappable && (pss > 0))
            {
                sharing_proportion = 0.0;
                if((shared_clean > 0) || (shared_dirty > 0))
                {
                    sharing_proportion = (pss - private_clean - private_dirty) / (shared_clean + shared_dirty);
                }
                swappable_pss = (size_t)((sharing_proportion * shared_clean) + private_clean);
            }
            else
            {
                swappable_pss = 0;
            }

            stats[which_heap].pss += pss;
            stats[which_heap].swappable_pss += swappable_pss;
            stats[which_heap].private_dirty += private_dirty;
            stats[which_heap].shared_dirty += shared_dirty;
            stats[which_heap].private_clean += private_clean;
            stats[which_heap].shared_clean += shared_clean;
            stats[which_heap].swapped_out += swapped_out;
            stats[which_heap].swapped_out_pss += swapped_out_pss;
            if (which_heap == HEAP_DALVIK ||
                which_heap == HEAP_DALVIK_OTHER ||
                which_heap == HEAP_DEX ||
                which_heap == HEAP_ART)
            {
                stats[sub_heap].pss += pss;
                stats[sub_heap].swappable_pss += swappable_pss;
                stats[sub_heap].private_dirty += private_dirty;
                stats[sub_heap].shared_dirty += shared_dirty;
                stats[sub_heap].private_clean += private_clean;
                stats[sub_heap].shared_clean += shared_clean;
                stats[sub_heap].swapped_out += swapped_out;
                stats[sub_heap].swapped_out_pss += swapped_out_pss;
            }
        }
    }
}

static int xcc_meminfo_record_from(int log_fd, const char *path, const char *title)
{
    FILE *fp = NULL;
    char  line[512];
    char *p;
    int   r = 0;

    if(NULL == (fp = fopen(path, "r"))) goto end;
    
    if(0 != (r = xcc_util_write_str(log_fd, title))) goto end;
    while(NULL != fgets(line, sizeof(line), fp))
    {
        p = xcc_util_trim(line);
        if(strlen(p) > 0)
        {
            if(0 != (r = xcc_util_write_format(log_fd, "  %s\n", p))) goto end;
        }
    }
    if(0 != (r = xcc_util_write_str(log_fd, "-\n"))) goto end;

 end:
    if(NULL != fp) fclose(fp);
    return r;
}

static int xcc_meminfo_record_sys(int log_fd)
{
    return xcc_meminfo_record_from(log_fd, "/proc/meminfo", " System Summary (From: /proc/meminfo)\n");
}

static int xcc_meminfo_record_proc_status(int log_fd, pid_t pid)
{
    char  path[64];
    snprintf(path, sizeof(path), "/proc/%d/status", pid);

    return xcc_meminfo_record_from(log_fd, path, " Process Status (From: /proc/PID/status)\n");
}

static int xcc_meminfo_record_proc_limits(int log_fd, pid_t pid)
{
    char  path[64];
    snprintf(path, sizeof(path), "/proc/%d/limits", pid);

    return xcc_meminfo_record_from(log_fd, path, " Process Limits (From: /proc/PID/limits)\n");
}

int xcc_meminfo_record(int log_fd, pid_t pid)
{
    char           path[64];
    FILE          *fp = NULL;
    xcc_meminfo_t  stats[_NUM_HEAP];
    xcc_meminfo_t  total;
    int            found_swap_pss = 0;
    size_t         i;
    int            r = 0;

    xcc_libc_support_memset(stats, 0, sizeof(stats));
    xcc_libc_support_memset(&total, 0, sizeof(total));

    //load memory info from /proc/pid/smaps
    snprintf(path, sizeof(path), "/proc/%d/smaps", pid);
    if(NULL == (fp = fopen(path, "r"))) return 0;
    xcc_meminfo_load(fp, stats, &found_swap_pss);
    fclose(fp);

    for(i = 0; i < _NUM_EXCLUSIVE_HEAP; i++)
    {
        total.pss += (stats[i].pss + stats[i].swapped_out_pss);
        total.swappable_pss += stats[i].swappable_pss;
        total.private_dirty += stats[i].private_dirty;
        total.shared_dirty += stats[i].shared_dirty;
        total.private_clean += stats[i].private_clean;
        total.shared_clean += stats[i].shared_clean;
        total.swapped_out += stats[i].swapped_out;
        total.swapped_out_pss += stats[i].swapped_out_pss;
    }

    //dump
    if(0 != (r = xcc_util_write_str(log_fd, "memory info:\n"))) return r;
    if(0 != (r = xcc_meminfo_record_sys(log_fd))) return r;
    if(0 != (r = xcc_meminfo_record_proc_status(log_fd, pid))) return r;
    if(0 != (r = xcc_meminfo_record_proc_limits(log_fd, pid))) return r;
    if(0 != (r = xcc_util_write_str(log_fd, " Process Details (From: /proc/PID/smaps)\n"))) return r;
    if(0 != (r = xcc_util_write_format(log_fd, XCC_MEMINFO_HEAD_FMT, "", "Pss", "Pss", "Shared", "Private", "Shared", "Private", found_swap_pss ? "SwapPss" : "Swap"))) return r;
    if(0 != (r = xcc_util_write_format(log_fd, XCC_MEMINFO_HEAD_FMT, "", "Total", "Clean", "Dirty", "Dirty", "Clean", "Clean", "Dirty"))) return r;
    if(0 != (r = xcc_util_write_format(log_fd, XCC_MEMINFO_HEAD_FMT, "", "------", "------", "------", "------", "------", "------", "------"))) return r;
    for(i = 0; i < _NUM_EXCLUSIVE_HEAP; i++)
    {
        if(HEAP_NATIVE == i || HEAP_DALVIK == i || HEAP_UNKNOWN == i ||
           0 != stats[i].pss || 0 != stats[i].swappable_pss || 0 != stats[i].shared_dirty ||
           0 != stats[i].private_dirty || 0 != stats[i].shared_clean || 0 != stats[i].private_clean ||
           0 != (found_swap_pss ? stats[i].swapped_out_pss : stats[i].swapped_out))
        {
            if(0 != (r = xcc_util_write_format(log_fd, XCC_MEMINFO_DATA_FMT, xcc_meminfo_label[i],
                                               stats[i].pss,
                                               stats[i].swappable_pss,
                                               stats[i].shared_dirty,
                                               stats[i].private_dirty,
                                               stats[i].shared_clean,
                                               stats[i].private_clean,
                                               found_swap_pss ? stats[i].swapped_out_pss : stats[i].swapped_out))) return r;
        }
    }
    if(0 != (r = xcc_util_write_format(log_fd, XCC_MEMINFO_DATA_FMT, "TOTAL",
                                       total.pss,
                                       total.swappable_pss,
                                       total.shared_dirty,
                                       total.private_dirty,
                                       total.shared_clean,
                                       total.private_clean,
                                       found_swap_pss ? total.swapped_out_pss : total.swapped_out))) return r;
    if(0 != (r = xcc_util_write_str(log_fd, "-\n Process Dalvik Details (From: /proc/PID/smaps)\n"))) return r;
    for(i = _NUM_EXCLUSIVE_HEAP; i < _NUM_HEAP; i++)
    {
        if(0 != stats[i].pss || 0 != stats[i].swappable_pss || 0 != stats[i].shared_dirty ||
           0 != stats[i].private_dirty || 0 != stats[i].shared_clean || 0 != stats[i].private_clean ||
           0 != (found_swap_pss ? stats[i].swapped_out_pss : stats[i].swapped_out))
        {
            if(0 != (r = xcc_util_write_format(log_fd, XCC_MEMINFO_DATA_FMT, xcc_meminfo_label[i],
                                               stats[i].pss,
                                               stats[i].swappable_pss,
                                               stats[i].shared_dirty,
                                               stats[i].private_dirty,
                                               stats[i].shared_clean,
                                               stats[i].private_clean,
                                               found_swap_pss ? stats[i].swapped_out_pss : stats[i].swapped_out))) return r;
        }
    }
    if(0 != (r = xcc_util_write_str(log_fd, "-\n Process Summary (From: /proc/PID/smaps)\n"))) return r;
    if(0 != (r = xcc_util_write_format(log_fd, XCC_MEMINFO_SUM_HEAD_FMT, "", "Pss(KB)"))) return r;
    if(0 != (r = xcc_util_write_format(log_fd, XCC_MEMINFO_SUM_HEAD_FMT, "", "------"))) return r;
    if(0 != (r = xcc_util_write_format(log_fd, XCC_MEMINFO_SUM_DATA_FMT, "Java Heap:",
                                       stats[HEAP_DALVIK].private_dirty +
                                       stats[HEAP_ART].private_dirty + stats[HEAP_ART].private_clean))) return r;
    if(0 != (r = xcc_util_write_format(log_fd, XCC_MEMINFO_SUM_DATA_FMT, "Native Heap:",
                                       stats[HEAP_NATIVE].private_dirty))) return r;
    if(0 != (r = xcc_util_write_format(log_fd, XCC_MEMINFO_SUM_DATA_FMT, "Code:",
                                       stats[HEAP_SO].private_dirty + stats[HEAP_SO].private_clean +
                                       stats[HEAP_JAR].private_dirty + stats[HEAP_JAR].private_clean +
                                       stats[HEAP_APK].private_dirty + stats[HEAP_APK].private_clean +
                                       stats[HEAP_TTF].private_dirty + stats[HEAP_TTF].private_clean +
                                       stats[HEAP_DEX].private_dirty + stats[HEAP_DEX].private_clean +
                                       stats[HEAP_OAT].private_dirty + stats[HEAP_OAT].private_clean))) return r;
    if(0 != (r = xcc_util_write_format(log_fd, XCC_MEMINFO_SUM_DATA_FMT, "Stack:",
                                       stats[HEAP_STACK].private_dirty))) return r;
    if(0 != (r = xcc_util_write_format(log_fd, XCC_MEMINFO_SUM_DATA_FMT, "Private Other:", 
                                       stats[HEAP_DALVIK_OTHER].private_dirty + stats[HEAP_DALVIK_OTHER].private_clean +
                                       stats[HEAP_CURSOR].private_dirty + stats[HEAP_CURSOR].private_clean +
                                       stats[HEAP_ASHMEM].private_dirty + stats[HEAP_ASHMEM].private_clean +
                                       stats[HEAP_GL_DEV].private_dirty + stats[HEAP_GL_DEV].private_clean +
                                       stats[HEAP_UNKNOWN_DEV].private_dirty + stats[HEAP_UNKNOWN_DEV].private_clean +
                                       stats[HEAP_UNKNOWN_MAP].private_dirty + stats[HEAP_UNKNOWN_MAP].private_clean +
                                       stats[HEAP_UNKNOWN].private_dirty + stats[HEAP_UNKNOWN].private_clean))) return r;
    if(0 != (r = xcc_util_write_format(log_fd, XCC_MEMINFO_SUM_DATA_FMT, "System:",
                                       total.pss - total.private_dirty - total.private_clean))) return r;
    if(found_swap_pss)
    {
        if(0 != (r = xcc_util_write_format(log_fd, XCC_MEMINFO_SUM_DATA2_FMT,
                                           "TOTAL:",
                                           total.pss,
                                           "TOTAL SWAP PSS:",
                                           total.swapped_out_pss))) return r;
    }
    else
    {
        if(0 != (r = xcc_util_write_format(log_fd, XCC_MEMINFO_SUM_DATA2_FMT,
                                           "TOTAL:",
                                           total.pss,
                                           "TOTAL SWAP:",
                                           total.swapped_out))) return r;
    }
    if(0 != (r = xcc_util_write_str(log_fd, "-\n\n"))) return r;
    
    return 0;
}
