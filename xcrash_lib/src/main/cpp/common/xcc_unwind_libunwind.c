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

#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/types.h>
#include <signal.h>
#include <ucontext.h>
#include <dlfcn.h>
#include <android/log.h>
#include "xcc_unwind_libunwind.h"
#include "xcc_fmt.h"
#include "xcc_util.h"

#define MAX_FRAMES 64

#if defined(__arm__)
#define UNW_TARGET "arm"
#define UNW_REG_IP 14
#define UNW_CURSOR_LEN 4096
#elif defined(__aarch64__)
#define UNW_TARGET "aarch64"
#define UNW_REG_IP 30
#define UNW_CURSOR_LEN 4096
#elif defined(__i386__)
#define UNW_TARGET "x86"
#define UNW_REG_IP 8
#define UNW_CURSOR_LEN 127
#elif defined(__x86_64__)
#define UNW_TARGET "x86_64"
#define UNW_REG_IP 16
#define UNW_CURSOR_LEN 127
#endif

typedef struct
{
    uintptr_t opaque[UNW_CURSOR_LEN];
} unw_cursor_t;

#if defined(__arm__)
typedef struct
{
    uintptr_t r[16];
} unw_context_t;
#else
typedef ucontext_t unw_context_t;
#endif

typedef int (*t_unw_init_local)(unw_cursor_t *, unw_context_t *);
typedef int (*t_unw_get_reg)(unw_cursor_t *, int, uintptr_t *);
typedef int (*t_unw_step)(unw_cursor_t *);

static void             *libunwind      = NULL;
static t_unw_init_local  unw_init_local = NULL;
static t_unw_get_reg     unw_get_reg    = NULL;
static t_unw_step        unw_step       = NULL;

void xcc_unwind_libunwind_init(void)
{
    if(NULL == (libunwind = dlopen("libunwind.so", RTLD_NOW))) return;
    if(NULL == (unw_init_local = (t_unw_init_local)dlsym(libunwind, "_U"UNW_TARGET"_init_local"))) goto err;
    if(NULL == (unw_get_reg = (t_unw_get_reg)dlsym(libunwind, "_U"UNW_TARGET"_get_reg"))) goto err;
    if(NULL == (unw_step = (t_unw_step)dlsym(libunwind, "_U"UNW_TARGET"_step"))) goto err;
    return;

 err:
    dlclose(libunwind);
    libunwind = NULL;
}

size_t xcc_unwind_libunwind_record(ucontext_t *uc, char *buf, size_t buf_len)
{
    unw_cursor_t  *cursor = NULL;
    unw_context_t *context = NULL;
    size_t         buf_used = 0, len, i = 0;
    uintptr_t      pc;
    Dl_info        info;

    if(NULL == libunwind) return 0;

    if(NULL == (cursor = calloc(1, sizeof(unw_cursor_t)))) return 0;
    if(NULL == (context = calloc(1, sizeof(unw_context_t)))) return 0;

#if defined(__arm__)
    context->r[0] = uc->uc_mcontext.arm_r0;
    context->r[1] = uc->uc_mcontext.arm_r1;
    context->r[2] = uc->uc_mcontext.arm_r2;
    context->r[3] = uc->uc_mcontext.arm_r3;
    context->r[4] = uc->uc_mcontext.arm_r4;
    context->r[5] = uc->uc_mcontext.arm_r5;
    context->r[6] = uc->uc_mcontext.arm_r6;
    context->r[7] = uc->uc_mcontext.arm_r7;
    context->r[8] = uc->uc_mcontext.arm_r8;
    context->r[9] = uc->uc_mcontext.arm_r9;
    context->r[10] = uc->uc_mcontext.arm_r10;
    context->r[11] = uc->uc_mcontext.arm_fp;
    context->r[12] = uc->uc_mcontext.arm_ip;
    context->r[13] = uc->uc_mcontext.arm_sp;
    context->r[14] = uc->uc_mcontext.arm_lr;
    context->r[15] = uc->uc_mcontext.arm_pc;
#else
    memcpy(context, uc, sizeof(ucontext_t));
#endif

    if(unw_init_local(cursor, context) < 0) goto end;
    do
    {
        //get current pc
        if(unw_get_reg(cursor, UNW_REG_IP, &pc) < 0) goto end;
        
        //append line for current frame
        if(0 == dladdr((void *)pc, &info) || (uintptr_t)info.dli_fbase > pc)
        {
            len = xcc_fmt_snprintf(buf + buf_used, buf_len - buf_used,
                                   "    #%02zu pc %0"XCC_UTIL_FMT_ADDR"  <unknown>\n",
                                   i, pc);
        }
        else
        {
            if(NULL == info.dli_fname || '\0' == info.dli_fname[0])
            {
                len = xcc_fmt_snprintf(buf + buf_used, buf_len - buf_used,
                                   "    #%02zu pc %0"XCC_UTIL_FMT_ADDR"  <anonymous:%"XCC_UTIL_FMT_ADDR">\n",
                                       i, pc - (uintptr_t)info.dli_fbase, (uintptr_t)info.dli_fbase);
            }
            else
            {
                if(NULL == info.dli_sname || '\0' == info.dli_sname[0])
                {
                    len = xcc_fmt_snprintf(buf + buf_used, buf_len - buf_used,
                                           "    #%02zu pc %0"XCC_UTIL_FMT_ADDR"  %s\n",
                                           i, pc - (uintptr_t)info.dli_fbase, info.dli_fname);
                }
                else
                {
                    if(0 == (uintptr_t)info.dli_saddr || (uintptr_t)info.dli_saddr > pc)
                    {
                        len = xcc_fmt_snprintf(buf + buf_used, buf_len - buf_used,
                                               "    #%02zu pc %0"XCC_UTIL_FMT_ADDR"  %s (%s)\n",
                                               i, pc - (uintptr_t)info.dli_fbase, info.dli_fname,
                                               info.dli_sname);
                    }
                    else
                    {
                        len = xcc_fmt_snprintf(buf + buf_used, buf_len - buf_used,
                                               "    #%02zu pc %0"XCC_UTIL_FMT_ADDR"  %s (%s+%"PRIuPTR")\n",
                                               i, pc - (uintptr_t)info.dli_fbase, info.dli_fname,
                                               info.dli_sname, pc - (uintptr_t)info.dli_saddr);
                    }
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

        i++;
        
    } while(unw_step(cursor) > 0 && i < MAX_FRAMES);
    
 end:
    if(NULL != cursor) free(cursor);
    if(NULL != context) free(context);
    return buf_used;
}
