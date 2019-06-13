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

#ifdef __x86_64__

#include <stdio.h>
#include <ucontext.h>
#include <sys/ptrace.h>
#include "xcc_errno.h"
#include "xcc_util.h"
#include "xcd_regs.h"
#include "xcd_memory.h"
#include "xcd_util.h"

#define XCD_REGS_RAX 0
#define XCD_REGS_RDX 1
#define XCD_REGS_RCX 2
#define XCD_REGS_RBX 3
#define XCD_REGS_RSI 4
#define XCD_REGS_RDI 5
#define XCD_REGS_RBP 6
#define XCD_REGS_RSP 7
#define XCD_REGS_R8  8
#define XCD_REGS_R9  9
#define XCD_REGS_R10 10
#define XCD_REGS_R11 11
#define XCD_REGS_R12 12
#define XCD_REGS_R13 13
#define XCD_REGS_R14 14
#define XCD_REGS_R15 15
#define XCD_REGS_RIP 16

#define XCD_REGS_SP  XCD_REGS_RSP
#define XCD_REGS_PC  XCD_REGS_RIP

static xcd_regs_label_t xcd_regs_labels[] =
{
    {XCD_REGS_RAX, "rax"},
    {XCD_REGS_RBX, "rbx"},
    {XCD_REGS_RCX, "rcx"},
    {XCD_REGS_RDX, "rdx"},
    {XCD_REGS_R8,  "r8"},
    {XCD_REGS_R9,  "r9"},
    {XCD_REGS_R10, "r10"},
    {XCD_REGS_R11, "r11"},
    {XCD_REGS_R12, "r12"},
    {XCD_REGS_R13, "r13"},
    {XCD_REGS_R14, "r14"},
    {XCD_REGS_R15, "r15"},
    {XCD_REGS_RDI, "rdi"},
    {XCD_REGS_RSI, "rsi"},
    {XCD_REGS_RBP, "rbp"},
    {XCD_REGS_RSP, "rsp"},
    {XCD_REGS_RIP, "rip"}
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
    self->r[XCD_REGS_RAX] = (uintptr_t)uc->uc_mcontext.gregs[REG_RAX];
    self->r[XCD_REGS_RBX] = (uintptr_t)uc->uc_mcontext.gregs[REG_RBX];
    self->r[XCD_REGS_RCX] = (uintptr_t)uc->uc_mcontext.gregs[REG_RCX];
    self->r[XCD_REGS_RDX] = (uintptr_t)uc->uc_mcontext.gregs[REG_RDX];
    self->r[XCD_REGS_R8]  = (uintptr_t)uc->uc_mcontext.gregs[REG_R8];
    self->r[XCD_REGS_R9]  = (uintptr_t)uc->uc_mcontext.gregs[REG_R9];
    self->r[XCD_REGS_R10] = (uintptr_t)uc->uc_mcontext.gregs[REG_R10];
    self->r[XCD_REGS_R11] = (uintptr_t)uc->uc_mcontext.gregs[REG_R11];
    self->r[XCD_REGS_R12] = (uintptr_t)uc->uc_mcontext.gregs[REG_R12];
    self->r[XCD_REGS_R13] = (uintptr_t)uc->uc_mcontext.gregs[REG_R13];
    self->r[XCD_REGS_R14] = (uintptr_t)uc->uc_mcontext.gregs[REG_R14];
    self->r[XCD_REGS_R15] = (uintptr_t)uc->uc_mcontext.gregs[REG_R15];
    self->r[XCD_REGS_RDI] = (uintptr_t)uc->uc_mcontext.gregs[REG_RDI];
    self->r[XCD_REGS_RSI] = (uintptr_t)uc->uc_mcontext.gregs[REG_RSI];
    self->r[XCD_REGS_RBP] = (uintptr_t)uc->uc_mcontext.gregs[REG_RBP];
    self->r[XCD_REGS_RSP] = (uintptr_t)uc->uc_mcontext.gregs[REG_RSP];
    self->r[XCD_REGS_RIP] = (uintptr_t)uc->uc_mcontext.gregs[REG_RIP];
}

void xcd_regs_load_from_ptregs(xcd_regs_t *self, uintptr_t *regs, size_t regs_len)
{
    (void)regs_len;
    
    struct pt_regs *ptregs = (struct pt_regs *)regs;

    self->r[XCD_REGS_RAX] = ptregs->rax;
    self->r[XCD_REGS_RBX] = ptregs->rbx;
    self->r[XCD_REGS_RCX] = ptregs->rcx;
    self->r[XCD_REGS_RDX] = ptregs->rdx;
    self->r[XCD_REGS_R8]  = ptregs->r8;
    self->r[XCD_REGS_R9]  = ptregs->r9;
    self->r[XCD_REGS_R10] = ptregs->r10;
    self->r[XCD_REGS_R11] = ptregs->r11;
    self->r[XCD_REGS_R12] = ptregs->r12;
    self->r[XCD_REGS_R13] = ptregs->r13;
    self->r[XCD_REGS_R14] = ptregs->r14;
    self->r[XCD_REGS_R15] = ptregs->r15;
    self->r[XCD_REGS_RDI] = ptregs->rdi;
    self->r[XCD_REGS_RSI] = ptregs->rsi;
    self->r[XCD_REGS_RBP] = ptregs->rbp;
    self->r[XCD_REGS_RSP] = ptregs->rsp;
    self->r[XCD_REGS_RIP] = ptregs->rip;
}

int xcd_regs_record(xcd_regs_t *self, int log_fd)
{
    return xcc_util_write_format(log_fd,
                                 "    rax %016lx  rbx %016lx  rcx %016lx  rdx %016lx\n"
                                 "    r8  %016lx  r9  %016lx  r10 %016lx  r11 %016lx\n"
                                 "    r12 %016lx  r13 %016lx  r14 %016lx  r15 %016lx\n"
                                 "    rdi %016lx  rsi %016lx\n"
                                 "    rbp %016lx  rsp %016lx  rip %016lx\n\n",
                                 self->r[XCD_REGS_RAX], self->r[XCD_REGS_RBX], self->r[XCD_REGS_RCX], self->r[XCD_REGS_RDX],
                                 self->r[XCD_REGS_R8],  self->r[XCD_REGS_R9],  self->r[XCD_REGS_R10], self->r[XCD_REGS_R11],
                                 self->r[XCD_REGS_R12], self->r[XCD_REGS_R13], self->r[XCD_REGS_R14], self->r[XCD_REGS_R15],
                                 self->r[XCD_REGS_RDI], self->r[XCD_REGS_RSI],
                                 self->r[XCD_REGS_RBP], self->r[XCD_REGS_RSP], self->r[XCD_REGS_RIP]);
}

int xcd_regs_try_step_sigreturn(xcd_regs_t *self, uintptr_t rel_pc, xcd_memory_t *memory, pid_t pid)
{
    uint64_t data = 0;
    if(0 != xcd_memory_read_fully(memory, rel_pc, &data, sizeof(data))) return XCC_ERRNO_MEM;
    if(0x0f0000000fc0c748 != data) return XCC_ERRNO_NOTFND;

    uint16_t data2 = 0;
    if(0 != xcd_memory_read_fully(memory, rel_pc + 8, &data2, sizeof(data2))) return XCC_ERRNO_MEM;
    if(0x0f05 != data2) return XCC_ERRNO_NOTFND;
    
    // __restore_rt:
    // 0x48 0xc7 0xc0 0x0f 0x00 0x00 0x00   mov $0xf,%rax
    // 0x0f 0x05                            syscall
    // 0x0f                                 nopl 0x0($rax)

    // Read the mcontext data from the stack.
    // sp points to the ucontext data structure, read only the mcontext part.
    ucontext_t uc;
    if(0 != xcd_util_ptrace_read_fully(pid, xcd_regs_get_sp(self) + 0x28, &(uc.uc_mcontext), sizeof(uc.uc_mcontext))) return XCC_ERRNO_MEM;
    xcd_regs_load_from_ucontext(self, &uc);
    
    return 0;
}

uintptr_t xcd_regs_get_adjust_pc(uintptr_t rel_pc, uintptr_t load_bias, xcd_memory_t *memory)
{    
    (void)load_bias;
    (void)memory;

    return 0 == rel_pc ? 0 : 1;
}

int xcd_regs_set_pc_from_lr(xcd_regs_t *self, pid_t pid)
{
    // Attempt to get the return address from the top of the stack.
    uintptr_t new_pc;
    if(0 != xcd_util_ptrace_read_fully(pid, xcd_regs_get_sp(self), &new_pc, sizeof(new_pc))) return XCC_ERRNO_MEM;
    
    if(new_pc == xcd_regs_get_pc(self)) return XCC_ERRNO_INVAL;
    
    xcd_regs_set_pc(self, new_pc);
    
    return 0;
}

#endif
