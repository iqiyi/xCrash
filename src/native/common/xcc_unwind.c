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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <inttypes.h>
#include <signal.h>
#include <ucontext.h>
#include <unwind.h>
#include <dlfcn.h>
#include <string.h>
#include <android/log.h>
#include "xcc_unwind.h"
#include "xcc_errno.h"
#include "xcc_util.h"
#include "xcc_fmt.h"

#define XCC_UNWIND_FRAMES_MAX 128

typedef struct
{
    uintptr_t *pc;
    uintptr_t *sp;
    size_t     cnt;
} xcc_unwind_state_t;

static _Unwind_Reason_Code xcc_unwind_callback(struct _Unwind_Context* context, void* arg)
{
    xcc_unwind_state_t* state = (xcc_unwind_state_t *)arg;
    uintptr_t pc = _Unwind_GetIP(context);
    uintptr_t sp = _Unwind_GetCFA(context);
    
    if(pc)
    {
        //If the pc and sp didn't change, then consider everything stopped.
        if(state->cnt > 0 && pc == *(state->pc - 1) && sp == *(state->sp - 1))
            return _URC_END_OF_STACK;
        
        *(state->pc) = pc;
        *(state->sp) = sp;
        state->pc++;
        state->sp++;
        
        state->cnt++;
        if(state->cnt >= XCC_UNWIND_FRAMES_MAX)
            return _URC_END_OF_STACK;
    }
    return _URC_NO_REASON;
}

static int xcc_unwind_check_ignore_lib(const char *name, const char *ignore_lib)
{
    size_t name_len, ignore_lib_len;
    
    if(NULL == ignore_lib) return 0;

    name_len = strlen(name);
    ignore_lib_len = strlen(ignore_lib);
    if(name_len < ignore_lib_len) return 0;

    return (0 == memcmp((void *)(name + name_len - ignore_lib_len), (void *)ignore_lib, ignore_lib_len) ? 1 : 0);
}

int xcc_unwind_get(ucontext_t *uc, const char *ignore_lib, char *buf, size_t buf_len)
{
    uintptr_t           sig_pc = 0;
    uintptr_t           pc_buf[XCC_UNWIND_FRAMES_MAX];
    uintptr_t           sp_buf[XCC_UNWIND_FRAMES_MAX];
    size_t              i, j;
    uintptr_t           rel_pc;
    const char         *name;
    const char         *symbol;
    uintptr_t           offset;
    Dl_info             info;
    xcc_unwind_state_t  state = {.pc = pc_buf, .sp = sp_buf, .cnt = 0};
    char                line[1024];
    size_t              len;
    int                 need_ignore = (ignore_lib ? 1 : 0);
    size_t              buf_used = 0;

    if(NULL == buf || 0 == buf_len) return 0;

#if defined(__arm__)
    sig_pc = uc->uc_mcontext.arm_pc;
#elif defined(__aarch64__)
    sig_pc = uc->uc_mcontext.pc;
#elif defined(__i386__)
    sig_pc = uc->uc_mcontext.gregs[REG_EIP];
#elif defined(__x86_64__)
    sig_pc = uc->uc_mcontext.gregs[REG_RIP];
#endif

    buf[0] = '\0';
    
    _Unwind_Backtrace(xcc_unwind_callback, &state);

    //find the signal frame
    for(i = 0; i < state.cnt; i++)
    {
        if(pc_buf[i] < sig_pc + sizeof(uintptr_t) && pc_buf[i] > sig_pc - sizeof(uintptr_t))
        {
            need_ignore = 0;
            break;
        }
    }
    if(i >= state.cnt) i = 0; //failed, record all frames

    for(j = 0; i < state.cnt; i++, j++)
    {
        rel_pc = pc_buf[i];
        name = "<unknown>";
        symbol = NULL;
        offset = 0;

        //parse PC
        if(dladdr((void *)(pc_buf[i]), &info))
        {
            rel_pc   = pc_buf[i] - (uintptr_t)info.dli_fbase;
            name = info.dli_fname;
            if(info.dli_sname)
            {
                symbol = info.dli_sname;
                offset = pc_buf[i] - (uintptr_t)info.dli_saddr;
            }
        }
        
        //check ignore lib
        if(need_ignore)
        {
            if(xcc_unwind_check_ignore_lib(name, ignore_lib))
                continue;
            else
                need_ignore = 0;
        }

        //build line
        len = xcc_fmt_snprintf(line, sizeof(line), "    #%02zu pc %0"XCC_UTIL_FMT_ADDR"  %s", j, rel_pc, name);
        if(NULL != symbol)
        {
            len += xcc_fmt_snprintf(line + len, sizeof(line) - len, " (%s", symbol);
            if(offset > 0)
                len += xcc_fmt_snprintf(line + len, sizeof(line) - len, "+%"PRIuPTR, offset);
            len += xcc_fmt_snprintf(line + len, sizeof(line) - len, ")");
        }
        len += xcc_fmt_snprintf(line + len, sizeof(line) - len, "\n");
        
        //copy to the buffer
        if(buf_used + len < buf_len)
        {
            memcpy(buf + buf_used, line, len);
            buf_used += len;
            buf[buf_used] = '\0';
        }
        else
        {
            break;
        }
    }

    return buf_used;
}
