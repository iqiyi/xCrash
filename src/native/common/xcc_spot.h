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

// Created by caikelun on 2019-03-07.

#ifndef XCC_SPOT_H
#define XCC_SPOT_H 1

#include <stdint.h>
#include <sys/types.h>
#include <ucontext.h>
#include <signal.h>

#ifdef __cplusplus
extern "C" {
#endif

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpadded"

typedef struct
{
    //set when crashed
    pid_t        crash_tid;
    siginfo_t    siginfo;
    ucontext_t   ucontext;
    uint64_t     crash_time;

    //set when inited
    int          api_level;
    pid_t        crash_pid;
    uint64_t     start_time;
    long         time_zone;
    unsigned int logcat_system_lines;
    unsigned int logcat_events_lines;
    unsigned int logcat_main_lines;
    int          dump_elf_hash;
    int          dump_map;
    int          dump_fds;
    int          dump_all_threads;
    unsigned int dump_all_threads_count_max;

    //set when crashed (content lenghts after this struct)
    size_t       log_pathname_len;
    
    //set when inited (content lenghts after this struct)
    size_t       os_version_len;
    size_t       kernel_version_len;
    size_t       abi_list_len;
    size_t       manufacturer_len;
    size_t       brand_len;
    size_t       model_len;
    size_t       build_fingerprint_len;
    size_t       app_id_len;
    size_t       app_version_len;
    size_t       dump_all_threads_whitelist_len;
} xcc_spot_t;

#pragma clang diagnostic pop

#ifdef __cplusplus
}
#endif

#endif
