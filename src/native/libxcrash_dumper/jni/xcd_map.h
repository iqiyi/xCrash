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

#ifndef XCD_MAP_H
#define XCD_MAP_H 1

#include <stdint.h>
#include <sys/types.h>
#include "xcd_elf.h"
#include "xcd_recorder.h"

#ifdef __cplusplus
extern "C" {
#endif

#define XCD_MAP_PORT_DEVICE 0x8000

typedef struct xcd_map
{
    //base info from /proc/<PID>/maps
    uintptr_t  start;
    uintptr_t  end;
    size_t     offset;
    uint16_t   flags;
    char      *name;

    //ELF
    xcd_elf_t *elf;
    size_t     elf_offset;
    int        elf_loaded;
} xcd_map_t;

int xcd_map_init(xcd_map_t *self, uintptr_t start, uintptr_t end, size_t offset, const char * flags, const char *name);
void xcd_map_uninit(xcd_map_t *self);

xcd_elf_t *xcd_map_get_elf(xcd_map_t *self, pid_t pid);
uintptr_t xcd_map_get_rel_pc(xcd_map_t *self, uintptr_t pc, pid_t pid);

int xcd_map_record(xcd_map_t *self, xcd_recorder_t *recorder);

#ifdef __cplusplus
}
#endif

#endif
