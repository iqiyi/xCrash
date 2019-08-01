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

#ifndef XCD_THREAD_H
#define XCD_THREAD_H 1

#include <stdint.h>
#include <sys/types.h>
#include "xcd_regs.h"
#include "xcd_frames.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    XCD_THREAD_STATUS_OK = 0,
    XCD_THREAD_STATUS_UNKNOWN,
    XCD_THREAD_STATUS_REGS,
    XCD_THREAD_STATUS_ATTACH,
    XCD_THREAD_STATUS_ATTACH_WAIT
} xcd_thread_status_t;

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpadded"
typedef struct xcd_thread
{
    xcd_thread_status_t  status;
    pid_t                pid;
    pid_t                tid;
    char                *tname;
    xcd_regs_t           regs;
    xcd_frames_t        *frames;
} xcd_thread_t;
#pragma clang diagnostic pop

void xcd_thread_init(xcd_thread_t *self, pid_t pid, pid_t tid);

void xcd_thread_suspend(xcd_thread_t *self);
void xcd_thread_resume(xcd_thread_t *self);

void xcd_thread_load_info(xcd_thread_t *self);
void xcd_thread_load_regs(xcd_thread_t *self);
void xcd_thread_load_regs_from_ucontext(xcd_thread_t *self, ucontext_t *uc);
int xcd_thread_load_frames(xcd_thread_t *self, xcd_maps_t *maps);

int xcd_thread_record_info(xcd_thread_t *self, int log_fd, const char *pname);
int xcd_thread_record_regs(xcd_thread_t *self, int log_fd);
int xcd_thread_record_backtrace(xcd_thread_t *self, int log_fd);
int xcd_thread_record_buildid(xcd_thread_t *self, int log_fd, int dump_elf_hash, uintptr_t fault_addr);
int xcd_thread_record_stack(xcd_thread_t *self, int log_fd);
int xcd_thread_record_memory(xcd_thread_t *self, int log_fd);

#ifdef __cplusplus
}
#endif

#endif
