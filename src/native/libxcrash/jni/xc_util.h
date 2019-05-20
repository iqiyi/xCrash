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

#ifndef XC_UTIL_H
#define XC_UTIL_H 1

#include <stdint.h>
#include <sys/types.h>
#include <time.h>
#include <sys/syscall.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __LP64__
#define XC_UTIL_SYSCALL_GETDENTS SYS_getdents
#else
#define XC_UTIL_SYSCALL_GETDENTS SYS_getdents64
#endif

typedef struct
{
#ifndef __LP64__
    unsigned long  d_ino;
    unsigned long  d_off;
    unsigned short d_reclen;
#else
    ino64_t        d_ino;
    off64_t        d_off;
    unsigned short d_reclen;
    unsigned char  d_type;
#endif
    char           d_name[1];
} xc_util_dirent_t;

struct tm *xca_util_time2tm(const time_t timev, long gmtoff, struct tm *result);
char *xc_util_strdupcat(const char *s1, const char *s2);
int xc_util_mkdirs(const char *dir);

#ifdef __cplusplus
}
#endif

#endif
