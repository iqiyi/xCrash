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

#include <stdint.h>
#include <sys/types.h>
#include <ucontext.h>
#include <android/log.h>
#include "xcc_unwind.h"
#include "xcc_unwind_libcorkscrew.h"
#include "xcc_unwind_libunwind.h"
#include "xcc_unwind_clang.h"

void xcc_unwind_init(int api_level)
{
#if defined(__arm__) || defined(__i386__)
    if(api_level >= 16 && api_level <= 20)
    {
        xcc_unwind_libcorkscrew_init();
    }
#endif
    
    if(api_level >= 21 && api_level <= 23)
    {
        xcc_unwind_libunwind_init();
    }
}

size_t xcc_unwind_get(int api_level, siginfo_t *si, ucontext_t *uc, char *buf, size_t buf_len)
{
    size_t buf_used;
    
#if defined(__arm__) || defined(__i386__)
    if(api_level >= 16 && api_level <= 20)
    {
        if(0 == (buf_used = xcc_unwind_libcorkscrew_record(si, uc, buf, buf_len))) goto bottom;
        return buf_used;
    }
#else
    (void)si;
#endif

    if(api_level >= 21 && api_level <= 23)
    {
        if(0 == (buf_used = xcc_unwind_libunwind_record(uc, buf, buf_len))) goto bottom;
        return buf_used;
    }

 bottom:
    return xcc_unwind_clang_record(uc, buf, buf_len);
}
