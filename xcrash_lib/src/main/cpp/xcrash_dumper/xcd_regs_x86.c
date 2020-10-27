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

#ifdef __i386__

#include <stdio.h>
#include <ucontext.h>
#include <sys/ptrace.h>
#include "xcc_errno.h"
#include "xcc_util.h"
#include "xcd_regs.h"
#include "xcd_memory.h"
#include "xcd_util.h"

#define XCD_REGS_EAX 0
#define XCD_REGS_ECX 1
#define XCD_REGS_EDX 2
#define XCD_REGS_EBX 3
#define XCD_REGS_ESP 4
#define XCD_REGS_EBP 5
#define XCD_REGS_ESI 6
#define XCD_REGS_EDI 7
#define XCD_REGS_EIP 8
//#define XCD_REGS_EFL 9
//#define XCD_REGS_CS  10
//#define XCD_REGS_SS  11
//#define XCD_REGS_DS  12
//#define XCD_REGS_ES  13
//#define XCD_REGS_FS  14
//#define XCD_REGS_GS  15

#define XCD_REGS_SP  XCD_REGS_ESP
#define XCD_REGS_PC  XCD_REGS_EIP

static xcd_regs_label_t xcd_regs_labels[] =
{
    {XCD_REGS_EAX, "eax"},
    {XCD_REGS_EBX, "ebx"},
    {XCD_REGS_ECX, "ecx"},
    {XCD_REGS_EDX, "edx"},
    {XCD_REGS_EDI, "edi"},
    {XCD_REGS_ESI, "esi"},
    {XCD_REGS_EBP, "ebp"},
    {XCD_REGS_ESP, "esp"},
    {XCD_REGS_EIP, "eip"}
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
    self->r[XCD_REGS_EAX] = (uintptr_t)uc->uc_mcontext.gregs[REG_EAX];
    self->r[XCD_REGS_EBX] = (uintptr_t)uc->uc_mcontext.gregs[REG_EBX];
    self->r[XCD_REGS_ECX] = (uintptr_t)uc->uc_mcontext.gregs[REG_ECX];
    self->r[XCD_REGS_EDX] = (uintptr_t)uc->uc_mcontext.gregs[REG_EDX];
    self->r[XCD_REGS_EDI] = (uintptr_t)uc->uc_mcontext.gregs[REG_EDI];
    self->r[XCD_REGS_ESI] = (uintptr_t)uc->uc_mcontext.gregs[REG_ESI];
    self->r[XCD_REGS_EBP] = (uintptr_t)uc->uc_mcontext.gregs[REG_EBP];
    self->r[XCD_REGS_ESP] = (uintptr_t)uc->uc_mcontext.gregs[REG_ESP];
    self->r[XCD_REGS_EIP] = (uintptr_t)uc->uc_mcontext.gregs[REG_EIP];
}

void xcd_regs_load_from_ptregs(xcd_regs_t *self, uintptr_t *regs, size_t regs_len)
{
    (void)regs_len;
    
    struct pt_regs *ptregs = (struct pt_regs *)regs;
    
    self->r[XCD_REGS_EAX] = (uintptr_t)ptregs->eax;
    self->r[XCD_REGS_EBX] = (uintptr_t)ptregs->ebx;
    self->r[XCD_REGS_ECX] = (uintptr_t)ptregs->ecx;
    self->r[XCD_REGS_EDX] = (uintptr_t)ptregs->edx;
    self->r[XCD_REGS_EDI] = (uintptr_t)ptregs->edi;
    self->r[XCD_REGS_ESI] = (uintptr_t)ptregs->esi;
    self->r[XCD_REGS_EBP] = (uintptr_t)ptregs->ebp;
    self->r[XCD_REGS_ESP] = (uintptr_t)ptregs->esp;
    self->r[XCD_REGS_EIP] = (uintptr_t)ptregs->eip;
}

int xcd_regs_record(xcd_regs_t *self, int log_fd)
{
    return xcc_util_write_format(log_fd,
                                 "    eax %08x  ebx %08x  ecx %08x  edx %08x\n"
                                 "    edi %08x  esi %08x\n"
                                 "    ebp %08x  esp %08x  eip %08x\n\n",
                                 self->r[XCD_REGS_EAX], self->r[XCD_REGS_EBX], self->r[XCD_REGS_ECX], self->r[XCD_REGS_EDX],
                                 self->r[XCD_REGS_EDI], self->r[XCD_REGS_ESI],
                                 self->r[XCD_REGS_EBP], self->r[XCD_REGS_ESP], self->r[XCD_REGS_EIP]);
}

int xcd_regs_try_step_sigreturn(xcd_regs_t *self, uintptr_t rel_pc, xcd_memory_t *memory, pid_t pid)
{
    uint64_t data;
    if(0 != xcd_memory_read_fully(memory, rel_pc, &data, sizeof(data))) return XCC_ERRNO_MEM;

    if(0x80cd00000077b858ULL == data)
    {
        // Without SA_SIGINFO set, the return sequence is:
        //
        //   __restore:
        //   0x58                            pop %eax
        //   0xb8 0x77 0x00 0x00 0x00        movl 0x77,%eax
        //   0xcd 0x80                       int 0x80
        //
        // SP points at arguments:
        //   int signum
        //   struct sigcontext (same format as mcontext)
        
        // Only read the portion of the data structure we care about.
        mcontext_t mc;
        if(0 != xcd_util_ptrace_read_fully(pid, xcd_regs_get_sp(self) + 4, &(mc.gregs), sizeof(mc.gregs))) return XCC_ERRNO_MEM;
        self->r[XCD_REGS_EAX] = (uintptr_t)mc.gregs[REG_EAX];
        self->r[XCD_REGS_EBX] = (uintptr_t)mc.gregs[REG_EBX];
        self->r[XCD_REGS_ECX] = (uintptr_t)mc.gregs[REG_ECX];
        self->r[XCD_REGS_EDX] = (uintptr_t)mc.gregs[REG_EDX];
        self->r[XCD_REGS_EBP] = (uintptr_t)mc.gregs[REG_EBP];
        self->r[XCD_REGS_ESP] = (uintptr_t)mc.gregs[REG_ESP];
        self->r[XCD_REGS_EIP] = (uintptr_t)mc.gregs[REG_EIP];
        
        return 0;
    }
    else if(0x0080cd000000adb8ULL == (data & 0x00ffffffffffffffULL))
    {
        // With SA_SIGINFO set, the return sequence is:
        //
        //   __restore_rt:
        //   0xb8 0xad 0x00 0x00 0x00        movl 0xad,%eax
        //   0xcd 0x80                       int 0x80
        //
        // SP points at arguments:
        //   int         signum
        //   siginfo_t  *siginfo
        //   ucontext_t *uc

        // Get the location of the ucontext_t data.
        uintptr_t p_uc;
        if(0 != xcd_util_ptrace_read_fully(pid, xcd_regs_get_sp(self) + 8, &p_uc, sizeof(p_uc))) return XCC_ERRNO_MEM;

        // Only read the portion of the data structure we care about.
        ucontext_t uc;
        if(0 != xcd_util_ptrace_read_fully(pid, p_uc + 0x14, &(uc.uc_mcontext), sizeof(uc.uc_mcontext))) return XCC_ERRNO_MEM;
        xcd_regs_load_from_ucontext(self, &uc);
        
        return 0;
    }

    return XCC_ERRNO_NOTFND;
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
