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

// Created by caikelun on 2019-08-02.

typedef int make_iso_compilers_happy;

#if defined(__arm__) || defined(__i386__)

#include <unistd.h>
#include <stdint.h>
#include <sys/types.h>
#include <signal.h>
#include <ucontext.h>
#include <dlfcn.h>
#include "xcc_unwind_libcorkscrew.h"
#include "xcc_fmt.h"
#include "xcc_util.h"

#define MAX_FRAMES 64

typedef struct map_info_t map_info_t;

typedef struct {
    uintptr_t absolute_pc;
    uintptr_t stack_top;
    size_t stack_size;
} backtrace_frame_t;

typedef struct {
    uintptr_t relative_pc;
    uintptr_t relative_symbol_addr;
    char* map_name;
    char* symbol_name;
    char* demangled_name;
} backtrace_symbol_t;

typedef ssize_t (*t_unwind_backtrace_signal_arch)(siginfo_t* si,
                                                  void* sc,
                                                  const map_info_t* lst,
                                                  backtrace_frame_t* bt,
                                                  size_t ignore_depth,
                                                  size_t max_depth);
typedef map_info_t* (*t_acquire_my_map_info_list)(void);
typedef void (*t_release_my_map_info_list)(map_info_t* milist);
typedef void (*t_get_backtrace_symbols)(const backtrace_frame_t* backtrace,
                                        size_t frames,
                                        backtrace_symbol_t* symbols);
typedef void (*t_free_backtrace_symbols)(backtrace_symbol_t* symbols,
                                         size_t frames);

static void                           *libcorkscrew                 = NULL;
static t_unwind_backtrace_signal_arch  unwind_backtrace_signal_arch = NULL;
static t_acquire_my_map_info_list      acquire_my_map_info_list     = NULL;
static t_release_my_map_info_list      release_my_map_info_list     = NULL;
static t_get_backtrace_symbols         get_backtrace_symbols        = NULL;
static t_free_backtrace_symbols        free_backtrace_symbols       = NULL;

void xcc_unwind_libcorkscrew_init(void)
{
    if(NULL == (libcorkscrew = dlopen("libcorkscrew.so", RTLD_NOW))) return;
    
    if(NULL == (unwind_backtrace_signal_arch = (t_unwind_backtrace_signal_arch)dlsym(libcorkscrew, "unwind_backtrace_signal_arch"))) goto err;
    if(NULL == (acquire_my_map_info_list = (t_acquire_my_map_info_list)dlsym(libcorkscrew, "acquire_my_map_info_list"))) goto err;
    release_my_map_info_list = (t_release_my_map_info_list)dlsym(libcorkscrew, "release_my_map_info_list");
    if(NULL == (get_backtrace_symbols = (t_get_backtrace_symbols)dlsym(libcorkscrew, "get_backtrace_symbols"))) goto err;
    free_backtrace_symbols = (t_free_backtrace_symbols)dlsym(libcorkscrew, "free_backtrace_symbols");
    return;

 err:
    dlclose(libcorkscrew);
    libcorkscrew = NULL;
}

size_t xcc_unwind_libcorkscrew_record(siginfo_t *si, ucontext_t *uc, char *buf, size_t buf_len)
{
    map_info_t         *map_info = NULL;
    backtrace_frame_t   frames[MAX_FRAMES];
    ssize_t             frames_used = 0, i;
    backtrace_symbol_t  symbols[MAX_FRAMES];
    size_t              buf_used = 0, len;

    if(NULL == libcorkscrew) return 0;

    //get frames
    if(NULL == (map_info = acquire_my_map_info_list())) goto end;
    if(0 >= (frames_used = unwind_backtrace_signal_arch(si, uc, map_info, frames, 0, MAX_FRAMES))) goto end;

    //get symbols
    get_backtrace_symbols(frames, (size_t)frames_used, symbols);

    for(i = 0; i < frames_used; i++)
    {
        //append line for current frame
        if(NULL == symbols[i].map_name || '\0' == symbols[i].map_name[0])
        {
            len = xcc_fmt_snprintf(buf + buf_used, buf_len - buf_used,
                                   "    #%02zu pc %0"XCC_UTIL_FMT_ADDR"  <unknown>\n",
                                   i, frames[i].absolute_pc);
        }
        else
        {
            if(NULL == symbols[i].symbol_name || '\0' == symbols[i].symbol_name[0])
            {
                len = xcc_fmt_snprintf(buf + buf_used, buf_len - buf_used,
                                       "    #%02zu pc %0"XCC_UTIL_FMT_ADDR"  %s\n",
                                       i, symbols[i].relative_pc, symbols[i].map_name);
            }
            else
            {
                if(0 == symbols[i].relative_symbol_addr)
                {
                    len = xcc_fmt_snprintf(buf + buf_used, buf_len - buf_used,
                                           "    #%02zu pc %0"XCC_UTIL_FMT_ADDR"  %s (%s)\n",
                                           i, symbols[i].relative_pc, symbols[i].map_name,
                                           symbols[i].symbol_name);
                }
                else
                {
                    len = xcc_fmt_snprintf(buf + buf_used, buf_len - buf_used,
                                           "    #%02zu pc %0"XCC_UTIL_FMT_ADDR"  %s (%s+%"PRIuPTR")\n",
                                           i, symbols[i].relative_pc, symbols[i].map_name,
                                           symbols[i].symbol_name, symbols[i].relative_symbol_addr);
                }
            }
        }

        //truncated?
        if(len >= buf_len - buf_used)
        {
            buf[buf_len - 2] = '\n';
            buf[buf_len - 1] = '\0';
            len = buf_len - buf_used - 1;
        }

        //check remaining space length in the buffer
        buf_used += len;
        if(buf_len - buf_used < 20) break;
    }

 end:
    if(NULL != release_my_map_info_list && NULL != map_info)
        release_my_map_info_list(map_info);
    if(NULL != free_backtrace_symbols && frames_used > 0)
        free_backtrace_symbols(symbols, (size_t)frames_used);
    
    return buf_used;
}

#endif
