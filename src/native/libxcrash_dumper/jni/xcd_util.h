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

#ifndef XCD_UTIL_H
#define XCD_UTIL_H 1

#include <stdint.h>
#include <inttypes.h>
#include <sys/types.h>
#include <signal.h>

#ifdef __cplusplus
extern "C" {
#endif

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpadded"
typedef struct
{
    int   api_level;
    char *os_version;
    char *abi_list;
    char *manufacturer;
    char *brand;
    char *model;
    char *build_fingerprint;
    char *revision;
} xcd_util_build_prop_t;
#pragma clang diagnostic pop

void xcd_util_load_build_prop(xcd_util_build_prop_t *prop);

int xcd_util_ptrace_read_long(pid_t pid, uintptr_t addr, long *value);
size_t xcd_util_ptrace_read(pid_t pid, uintptr_t addr, void *dst, size_t bytes);
int xcd_util_ptrace_read_fully(pid_t pid, uintptr_t addr, void *dst, size_t bytes);

int xcd_util_xz_decompress(uint8_t* src, size_t src_size, uint8_t** dst, size_t* dst_size);

#ifdef __cplusplus
}
#endif

#endif
