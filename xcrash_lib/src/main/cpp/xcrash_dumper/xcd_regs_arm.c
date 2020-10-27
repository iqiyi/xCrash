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

#ifdef __arm__

#include <stdio.h>
#include <string.h>
#include <ucontext.h>
#include "xcc_errno.h"
#include "xcc_util.h"
#include "xcd_regs.h"
#include "xcd_memory.h"
#include "xcd_util.h"

#define XCD_REGS_R0   0
#define XCD_REGS_R1   1
#define XCD_REGS_R2   2
#define XCD_REGS_R3   3
#define XCD_REGS_R4   4
#define XCD_REGS_R5   5
#define XCD_REGS_R6   6
#define XCD_REGS_R7   7
#define XCD_REGS_R8   8
#define XCD_REGS_R9   9
#define XCD_REGS_R10  10
#define XCD_REGS_R11  11
#define XCD_REGS_IP   12
#define XCD_REGS_SP   13
#define XCD_REGS_LR   14
#define XCD_REGS_PC   15

static xcd_regs_label_t xcd_regs_labels[] =
{
    {XCD_REGS_R0,  "r0"},
    {XCD_REGS_R1,  "r1"},
    {XCD_REGS_R2,  "r2"},
    {XCD_REGS_R3,  "r3"},
    {XCD_REGS_R4,  "r4"},
    {XCD_REGS_R5,  "r5"},
    {XCD_REGS_R6,  "r6"},
    {XCD_REGS_R7,  "r7"},
    {XCD_REGS_R8,  "r8"},
    {XCD_REGS_R9,  "r9"},
    {XCD_REGS_R10, "r10"},
    {XCD_REGS_R11, "r11"},
    {XCD_REGS_IP,  "ip"},
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
    self->r[XCD_REGS_R0]  = uc->uc_mcontext.arm_r0;
    self->r[XCD_REGS_R1]  = uc->uc_mcontext.arm_r1;
    self->r[XCD_REGS_R2]  = uc->uc_mcontext.arm_r2;
    self->r[XCD_REGS_R3]  = uc->uc_mcontext.arm_r3;
    self->r[XCD_REGS_R4]  = uc->uc_mcontext.arm_r4;
    self->r[XCD_REGS_R5]  = uc->uc_mcontext.arm_r5;
    self->r[XCD_REGS_R6]  = uc->uc_mcontext.arm_r6;
    self->r[XCD_REGS_R7]  = uc->uc_mcontext.arm_r7;
    self->r[XCD_REGS_R8]  = uc->uc_mcontext.arm_r8;
    self->r[XCD_REGS_R9]  = uc->uc_mcontext.arm_r9;
    self->r[XCD_REGS_R10] = uc->uc_mcontext.arm_r10;
    self->r[XCD_REGS_R11] = uc->uc_mcontext.arm_fp;
    self->r[XCD_REGS_IP]  = uc->uc_mcontext.arm_ip;
    self->r[XCD_REGS_SP]  = uc->uc_mcontext.arm_sp;
    self->r[XCD_REGS_LR]  = uc->uc_mcontext.arm_lr;
    self->r[XCD_REGS_PC]  = uc->uc_mcontext.arm_pc;
}

void xcd_regs_load_from_ptregs(xcd_regs_t *self, uintptr_t *regs, size_t regs_len)
{
    if(regs_len > XCD_REGS_USER_NUM) regs_len = XCD_REGS_USER_NUM;
    memcpy(&(self->r), regs, sizeof(uintptr_t) * regs_len);
}

int xcd_regs_record(xcd_regs_t *self, int log_fd)
{
    return xcc_util_write_format(log_fd,
                                 "    r0  %08x  r1  %08x  r2  %08x  r3  %08x\n"
                                 "    r4  %08x  r5  %08x  r6  %08x  r7  %08x\n"
                                 "    r8  %08x  r9  %08x  r10 %08x  r11 %08x\n"
                                 "    ip  %08x  sp  %08x  lr  %08x  pc  %08x\n\n",
                                 self->r[XCD_REGS_R0], self->r[XCD_REGS_R1], self->r[XCD_REGS_R2],  self->r[XCD_REGS_R3],
                                 self->r[XCD_REGS_R4], self->r[XCD_REGS_R5], self->r[XCD_REGS_R6],  self->r[XCD_REGS_R7],
                                 self->r[XCD_REGS_R8], self->r[XCD_REGS_R9], self->r[XCD_REGS_R10], self->r[XCD_REGS_R11],
                                 self->r[XCD_REGS_IP], self->r[XCD_REGS_SP], self->r[XCD_REGS_LR],  self->r[XCD_REGS_PC]);
}

int xcd_regs_try_step_sigreturn(xcd_regs_t *self, uintptr_t rel_pc, xcd_memory_t *memory, pid_t pid)
{
    uint32_t  data;
    uintptr_t offset = 0;
    uintptr_t sp;

    if(0 != xcd_memory_read_fully(memory, rel_pc, &data, sizeof(data))) return XCC_ERRNO_MEM;
    
    if(data == 0xe3a07077 || data == 0xef900077 || data == 0xdf002777)
    {
        // non-RT sigreturn call.
        // __restore:
        //
        // Form 1 (arm EABI):
        // 0x77 0x70              mov r7, #0x77
        // 0xa0 0xe3              svc 0x00000000
        //
        // Form 2 (arm OABI):
        // 0x77 0x00 0x90 0xef    svc 0x00900077
        //
        // Form 3 (thumb):
        // 0x77 0x27              movs r7, #77
        // 0x00 0xdf              svc 0
        
        sp = xcd_regs_get_sp(self);
        if(0 != xcd_util_ptrace_read_fully(pid, sp, &data, sizeof(data))) return XCC_ERRNO_MEM;
        if(0x5ac3c35a == data)
        {
            // SP + uc_mcontext_offset + r0_offset.
            offset = sp + 0x14 + 0xc;
        }
        else
        {
            // SP + r0_offset
            offset = sp + 0xc;
        }
    }
    else if(data == 0xe3a070ad || data == 0xef9000ad || data == 0xdf0027ad)
    {
        // RT sigreturn call.
        // __restore_rt:
        //
        // Form 1 (arm EABI):
        // 0xad 0x70              mov r7, #0xad
        // 0xa0 0xe3              svc 0x00000000
        //
        // Form 2 (arm OABI):
        // 0xad 0x00 0x90 0xef    svc 0x009000ad
        //
        // Form 3 (thumb):
        // 0xad 0x27              movs r7, #ad
        // 0x00 0xdf              svc 0
        
        sp = xcd_regs_get_sp(self);
        if(0 != xcd_util_ptrace_read_fully(pid, sp, &data, sizeof(data))) return XCC_ERRNO_MEM;
        if(data == sp + 8)
        {
            // SP + 8 + sizeof(siginfo_t) + uc_mcontext_offset + r0 offset
            offset = sp + 8 + 0x80 + 0x14 + 0xc;
        }
        else
        {
            // SP + sizeof(siginfo_t) + uc_mcontext_offset + r0 offset
            offset = sp + 0x80 + 0x14 + 0xc;
        }
    }
    
    if(0 == offset) return XCC_ERRNO_NOTFND;

    if(0 != xcd_util_ptrace_read_fully(pid, offset, self, sizeof(uint32_t) * XCD_REGS_MACHINE_NUM)) return XCC_ERRNO_MEM;
    
    return 0;
}

uintptr_t xcd_regs_get_adjust_pc(uintptr_t rel_pc, uintptr_t load_bias, xcd_memory_t *memory)
{
    if(NULL == memory) return 2;
    
    if(rel_pc < load_bias)
    {
        if(rel_pc < 2)
        {
            return 0;
        }
        return 2;
    }

    uint64_t adjusted_rel_pc = rel_pc - load_bias;
    if(adjusted_rel_pc < 5)
    {
        if(adjusted_rel_pc < 2)
        {
            return 0;
        }
        return 2;
    }

    if(adjusted_rel_pc & 1)
    {
        // This is a thumb instruction, it could be 2 or 4 bytes.
        uint32_t value;
        if(0 != xcd_memory_read_fully(memory, (uintptr_t)(adjusted_rel_pc - 5), &value, sizeof(value)) ||
           (value & 0xe000f000) != 0xe000f000)
        {
            return 2;
        }
    }
    
    return 4;
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
