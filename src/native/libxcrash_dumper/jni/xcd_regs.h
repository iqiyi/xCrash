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

#ifndef XCD_REGS_H
#define XCD_REGS_H 1

#include <stdint.h>
#include <sys/types.h>
#include <ucontext.h>
#include "xcd_memory.h"

#ifdef __cplusplus
extern "C" {
#endif

#if defined(__arm__)
#define XCD_REGS_USER_NUM    18
#define XCD_REGS_MACHINE_NUM 16
#elif defined(__aarch64__)
#define XCD_REGS_USER_NUM    34
#define XCD_REGS_MACHINE_NUM 33
#elif defined(__i386__)
#define XCD_REGS_USER_NUM    17
#define XCD_REGS_MACHINE_NUM 16
#elif defined(__x86_64__)
#define XCD_REGS_USER_NUM    27
#define XCD_REGS_MACHINE_NUM 17
#endif

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpadded"
typedef struct {
    uintptr_t r[XCD_REGS_USER_NUM];
} xcd_regs_t;
#pragma clang diagnostic pop

uintptr_t xcd_regs_get_pc(xcd_regs_t *self);
void xcd_regs_set_pc(xcd_regs_t *self, uintptr_t pc);

uintptr_t xcd_regs_get_sp(xcd_regs_t *self);
void xcd_regs_set_sp(xcd_regs_t *self, uintptr_t sp);

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpadded"
typedef struct {
    uint8_t     idx;
    const char *name;
} xcd_regs_label_t;
#pragma clang diagnostic pop

void xcd_regs_get_labels(xcd_regs_label_t **labels, size_t *labels_count);

void xcd_regs_load_from_ucontext(xcd_regs_t *self, ucontext_t *uc);
void xcd_regs_load_from_ptregs(xcd_regs_t *self, uintptr_t *regs, size_t regs_len);

int xcd_regs_record(xcd_regs_t *self, int log_fd);

int xcd_regs_try_step_sigreturn(xcd_regs_t *self, uintptr_t rel_pc, xcd_memory_t *memory, pid_t pid);

uintptr_t xcd_regs_get_adjust_pc(uintptr_t rel_pc, uintptr_t load_bias, xcd_memory_t *memory);

int xcd_regs_set_pc_from_lr(xcd_regs_t *self, pid_t pid);

#ifdef __cplusplus
}
#endif

#endif
