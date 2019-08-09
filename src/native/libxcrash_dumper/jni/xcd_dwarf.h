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

#ifndef XCD_DWARF_H
#define XCD_DWARF_H 1

#include <stdint.h>
#include <sys/types.h>
#include "xcd_regs.h"
#include "xcd_memory.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    XCD_DWARF_TYPE_DEBUG_FRAME,
    XCD_DWARF_TYPE_EH_FRAME,
    XCD_DWARF_TYPE_EH_FRAME_HDR
} xcd_dwarf_type_t;

typedef struct xcd_dwarf xcd_dwarf_t;

int xcd_dwarf_create(xcd_dwarf_t **self, xcd_memory_t *memory, pid_t pid, uintptr_t load_bias,
                     size_t offset, size_t size, xcd_dwarf_type_t type);

int xcd_dwarf_step(xcd_dwarf_t *self, xcd_regs_t *regs, uintptr_t pc, int *finished);


#ifdef __cplusplus
}
#endif

#endif
