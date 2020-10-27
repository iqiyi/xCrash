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

#ifndef XCD_FRAMES_H
#define XCD_FRAMES_H 1

#include <stdint.h>
#include <sys/types.h>
#include "xcd_regs.h"
#include "xcd_maps.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct xcd_frames xcd_frames_t;

int xcd_frames_create(xcd_frames_t **self, xcd_regs_t *regs, xcd_maps_t *maps, pid_t pid);
void xcd_frames_destroy(xcd_frames_t **self);

int xcd_frames_record_backtrace(xcd_frames_t *self, int log_fd);
int xcd_frames_record_buildid(xcd_frames_t *self, int log_fd, int dump_elf_hash, uintptr_t fault_addr);
int xcd_frames_record_stack(xcd_frames_t *self, int log_fd);

#ifdef __cplusplus
}
#endif

#endif
