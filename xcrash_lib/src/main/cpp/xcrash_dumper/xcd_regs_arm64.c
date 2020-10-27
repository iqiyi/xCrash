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

typedef int make_iso_compilers_happy;

#ifdef __aarch64__

#include <stdio.h>
#include <string.h>
#include <ucontext.h>
#include "xcc_errno.h"
#include "xcc_util.h"
#include "xcd_regs.h"
#include "xcd_memory.h"
#include "xcd_util.h"

#define XCD_REGS_X0  0
#define XCD_REGS_X1  1
#define XCD_REGS_X2  2
#define XCD_REGS_X3  3
#define XCD_REGS_X4  4
#define XCD_REGS_X5  5
#define XCD_REGS_X6  6
#define XCD_REGS_X7  7
#define XCD_REGS_X8  8
#define XCD_REGS_X9  9
#define XCD_REGS_X10 10
#define XCD_REGS_X11 11
#define XCD_REGS_X12 12
#define XCD_REGS_X13 13
#define XCD_REGS_X14 14
#define XCD_REGS_X15 15
#define XCD_REGS_X16 16
#define XCD_REGS_X17 17
#define XCD_REGS_X18 18
#define XCD_REGS_X19 19
#define XCD_REGS_X20 20
#define XCD_REGS_X21 21
#define XCD_REGS_X22 22
#define XCD_REGS_X23 23
#define XCD_REGS_X24 24
#define XCD_REGS_X25 25
#define XCD_REGS_X26 26
#define XCD_REGS_X27 27
#define XCD_REGS_X28 28
#define XCD_REGS_X29 29
#define XCD_REGS_LR  30
#define XCD_REGS_SP  31
#define XCD_REGS_PC  32

static xcd_regs_label_t xcd_regs_labels[] =
{
    {XCD_REGS_X0,  "x0"},
    {XCD_REGS_X1,  "x1"},
    {XCD_REGS_X2,  "x2"},
    {XCD_REGS_X3,  "x3"},
    {XCD_REGS_X4,  "x4"},
    {XCD_REGS_X5,  "x5"},
    {XCD_REGS_X6,  "x6"},
    {XCD_REGS_X7,  "x7"},
    {XCD_REGS_X8,  "x8"},
    {XCD_REGS_X9,  "x9"},
    {XCD_REGS_X10, "x10"},
    {XCD_REGS_X11, "x11"},
    {XCD_REGS_X12, "x12"},
    {XCD_REGS_X13, "x13"},
    {XCD_REGS_X14, "x14"},
    {XCD_REGS_X15, "x15"},
    {XCD_REGS_X16, "x16"},
    {XCD_REGS_X17, "x17"},
    {XCD_REGS_X18, "x18"},
    {XCD_REGS_X19, "x19"},
    {XCD_REGS_X20, "x20"},
    {XCD_REGS_X21, "x21"},
    {XCD_REGS_X22, "x22"},
    {XCD_REGS_X23, "x23"},
    {XCD_REGS_X24, "x24"},
    {XCD_REGS_X25, "x25"},
    {XCD_REGS_X26, "x26"},
    {XCD_REGS_X27, "x27"},
    {XCD_REGS_X28, "x28"},
    {XCD_REGS_X29, "x29"},
    {XCD_REGS_SP,  "sp"},
    {XCD_REGS_LR,  "lr"},
    {XCD_REGS_PC,  "pc"}
};

void xcd_regs_get_labels(xcd_regs_label_t **labels, size_t *labels_count)
{
    *labels = xcd_regs_labels;
    *labels_count = sizeof(xcd_regs_labels) / sizeof(xcd_regs_label_t);
}

uintptr_t xcd_regs_get_pc(xcd_regs_t *self)
{
    return self->r[XCD_REGS_PC];
}

void xcd_regs_set_pc(xcd_regs_t *self, uintptr_t pc)
{
    self->r[XCD_REGS_PC] = pc;
}

uintptr_t xcd_regs_get_sp(xcd_regs_t *self)
{
    return self->r[XCD_REGS_SP];
}

void xcd_regs_set_sp(xcd_regs_t *self, uintptr_t sp)
{
    self->r[XCD_REGS_SP] = sp;
}

void xcd_regs_load_from_ucontext(xcd_regs_t *self, ucontext_t *uc)
{
    self->r[XCD_REGS_X0]  = uc->uc_mcontext.regs[0];
    self->r[XCD_REGS_X1]  = uc->uc_mcontext.regs[1];
    self->r[XCD_REGS_X2]  = uc->uc_mcontext.regs[2];
    self->r[XCD_REGS_X3]  = uc->uc_mcontext.regs[3];
    self->r[XCD_REGS_X4]  = uc->uc_mcontext.regs[4];
    self->r[XCD_REGS_X5]  = uc->uc_mcontext.regs[5];
    self->r[XCD_REGS_X6]  = uc->uc_mcontext.regs[6];
    self->r[XCD_REGS_X7]  = uc->uc_mcontext.regs[7];
    self->r[XCD_REGS_X8]  = uc->uc_mcontext.regs[8];
    self->r[XCD_REGS_X9]  = uc->uc_mcontext.regs[9];
    self->r[XCD_REGS_X10] = uc->uc_mcontext.regs[10];
    self->r[XCD_REGS_X11] = uc->uc_mcontext.regs[11];
    self->r[XCD_REGS_X12] = uc->uc_mcontext.regs[12];
    self->r[XCD_REGS_X13] = uc->uc_mcontext.regs[13];
    self->r[XCD_REGS_X14] = uc->uc_mcontext.regs[14];
    self->r[XCD_REGS_X15] = uc->uc_mcontext.regs[15];
    self->r[XCD_REGS_X16] = uc->uc_mcontext.regs[16];
    self->r[XCD_REGS_X17] = uc->uc_mcontext.regs[17];
    self->r[XCD_REGS_X18] = uc->uc_mcontext.regs[18];
    self->r[XCD_REGS_X19] = uc->uc_mcontext.regs[19];
    self->r[XCD_REGS_X20] = uc->uc_mcontext.regs[20];
    self->r[XCD_REGS_X21] = uc->uc_mcontext.regs[21];
    self->r[XCD_REGS_X22] = uc->uc_mcontext.regs[22];
    self->r[XCD_REGS_X23] = uc->uc_mcontext.regs[23];
    self->r[XCD_REGS_X24] = uc->uc_mcontext.regs[24];
    self->r[XCD_REGS_X25] = uc->uc_mcontext.regs[25];
    self->r[XCD_REGS_X26] = uc->uc_mcontext.regs[26];
    self->r[XCD_REGS_X27] = uc->uc_mcontext.regs[27];
    self->r[XCD_REGS_X28] = uc->uc_mcontext.regs[28];
    self->r[XCD_REGS_X29] = uc->uc_mcontext.regs[29];
    self->r[XCD_REGS_LR]  = uc->uc_mcontext.regs[30];
    self->r[XCD_REGS_SP]  = uc->uc_mcontext.sp;
    self->r[XCD_REGS_PC]  = uc->uc_mcontext.pc;
}

void xcd_regs_load_from_ptregs(xcd_regs_t *self, uintptr_t *regs, size_t regs_len)
{
    if(regs_len > XCD_REGS_USER_NUM) regs_len = XCD_REGS_USER_NUM;
    memcpy(&(self->r), regs, sizeof(uintptr_t) * regs_len);
}

int xcd_regs_record(xcd_regs_t *self, int log_fd)
{
    return xcc_util_write_format(log_fd,
                                 "    x0  %016lx  x1  %016lx  x2  %016lx  x3  %016lx\n"
                                 "    x4  %016lx  x5  %016lx  x6  %016lx  x7  %016lx\n"
                                 "    x8  %016lx  x9  %016lx  x10 %016lx  x11 %016lx\n"
                                 "    x12 %016lx  x13 %016lx  x14 %016lx  x15 %016lx\n"
                                 "    x16 %016lx  x17 %016lx  x18 %016lx  x19 %016lx\n"
                                 "    x20 %016lx  x21 %016lx  x22 %016lx  x23 %016lx\n"
                                 "    x24 %016lx  x25 %016lx  x26 %016lx  x27 %016lx\n"
                                 "    x28 %016lx  x29 %016lx\n"
                                 "    sp  %016lx  lr  %016lx  pc  %016lx\n\n",
                                 self->r[XCD_REGS_X0],  self->r[XCD_REGS_X1],  self->r[XCD_REGS_X2],  self->r[XCD_REGS_X3],
                                 self->r[XCD_REGS_X4],  self->r[XCD_REGS_X5],  self->r[XCD_REGS_X6],  self->r[XCD_REGS_X7],
                                 self->r[XCD_REGS_X8],  self->r[XCD_REGS_X9],  self->r[XCD_REGS_X10], self->r[XCD_REGS_X11],
                                 self->r[XCD_REGS_X12], self->r[XCD_REGS_X13], self->r[XCD_REGS_X14], self->r[XCD_REGS_X15],
                                 self->r[XCD_REGS_X16], self->r[XCD_REGS_X17], self->r[XCD_REGS_X18], self->r[XCD_REGS_X19],
                                 self->r[XCD_REGS_X20], self->r[XCD_REGS_X21], self->r[XCD_REGS_X22], self->r[XCD_REGS_X23],
                                 self->r[XCD_REGS_X24], self->r[XCD_REGS_X25], self->r[XCD_REGS_X26], self->r[XCD_REGS_X27],
                                 self->r[XCD_REGS_X28], self->r[XCD_REGS_X29],
                                 self->r[XCD_REGS_SP],  self->r[XCD_REGS_LR],  self->r[XCD_REGS_PC]);
}

int xcd_regs_try_step_sigreturn(xcd_regs_t *self, uintptr_t rel_pc, xcd_memory_t *memory, pid_t pid)
{
    uint64_t  data;
    uintptr_t offset;

    if(0 != xcd_memory_read_fully(memory, rel_pc, &data, sizeof(data))) return XCC_ERRNO_MEM;

    // Look for the kernel sigreturn function.
    // __kernel_rt_sigreturn:
    // 0xd2801168     mov x8, #0x8b
    // 0xd4000001     svc #0x0
    if (0xd4000001d2801168ULL != data) return XCC_ERRNO_NOTFND;

    // SP + sizeof(siginfo_t) + uc_mcontext offset + X0 offset.
    offset = self->r[XCD_REGS_SP] + 0x80 + 0xb0 + 0x08;
    if(0 != xcd_util_ptrace_read_fully(pid, offset, self, sizeof(uint64_t) * XCD_REGS_MACHINE_NUM)) return XCC_ERRNO_MEM;
    
    return 0;
}

uintptr_t xcd_regs_get_adjust_pc(uintptr_t rel_pc, uintptr_t load_bias, xcd_memory_t *memory)
{
    (void)load_bias;
    (void)memory;

    return rel_pc < 4 ? 0 : 4;
}

int xcd_regs_set_pc_from_lr(xcd_regs_t *self, pid_t pid)
{
    (void)pid;

    if(self->r[XCD_REGS_PC] == self->r[XCD_REGS_LR])
        return XCC_ERRNO_INVAL;

    self->r[XCD_REGS_PC] = self->r[XCD_REGS_LR];
    return 0;
}

#endif
