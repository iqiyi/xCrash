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

#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <link.h>
#include <elf.h>
#include <sys/types.h>
#include "xcc_errno.h"
#include "xcd_elf.h"
#include "xcd_elf_interface.h"
#include "xcd_memory.h"
#include "xcd_log.h"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpadded"
struct xcd_elf
{
    pid_t                pid;
    xcd_memory_t        *memory;
    uintptr_t            load_bias;
    xcd_elf_interface_t *interface;
    xcd_elf_interface_t *gnu_interface;
    int                  gnu_interface_created;
};
#pragma clang diagnostic pop

int xcd_elf_is_valid(xcd_memory_t *memory)
{
    if(NULL == memory) return 0;

    uint8_t e_ident[SELFMAG + 1];
    if(0 != xcd_memory_read_fully(memory, 0, e_ident, SELFMAG)) return 0;
    if(0 != memcmp(e_ident, ELFMAG, SELFMAG)) return 0;

    uint8_t class_type;
    if(0 != xcd_memory_read_fully(memory, EI_CLASS, &class_type, 1)) return 0;
#if defined(__LP64__)
    if(ELFCLASS64 != class_type) return 0;
#else
    if(ELFCLASS32 != class_type) return 0;
#endif

    return 1;
}

size_t xcd_elf_get_max_size(xcd_memory_t *memory)
{
    ElfW(Ehdr) ehdr;

    if(0 != xcd_memory_read_fully(memory, 0, &ehdr, sizeof(ehdr))) return 0;
    if(0 == ehdr.e_shnum) return 0;
    
    return ehdr.e_shoff + ehdr.e_shentsize * ehdr.e_shnum;
}

int xcd_elf_create(xcd_elf_t **self, pid_t pid, xcd_memory_t *memory)
{
    int r;
    
    if(NULL == (*self = calloc(1, sizeof(xcd_elf_t)))) return XCC_ERRNO_NOMEM;
    (*self)->pid = pid;
    (*self)->memory = memory;

    //create ELF interface, save load bias
    if(0 != (r = xcd_elf_interface_create(&((*self)->interface), pid, memory, &((*self)->load_bias))))
    {
        free(*self);
        return r;
    }

    return 0;
}

uintptr_t xcd_elf_get_load_bias(xcd_elf_t *self)
{
    return self->load_bias;
}

xcd_memory_t *xcd_elf_get_memory(xcd_elf_t *self)
{
    return self->memory;
}

int xcd_elf_step(xcd_elf_t *self, uintptr_t rel_pc, uintptr_t step_pc, xcd_regs_t *regs, int *finished, int *sigreturn)
{
    *finished = 0;
    *sigreturn = 0;
    
    //try sigreturn
    if(0 == xcd_regs_try_step_sigreturn(regs, rel_pc, self->memory, self->pid))
    {
        *finished = 0;
        *sigreturn = 1;
#if XCD_ELF_DEBUG
        XCD_LOG_DEBUG("ELF: step by sigreturn OK, rel_pc=%"PRIxPTR", finished=0", rel_pc);
#endif
        return 0;
    }

    //try DWARF (.debug_frame and .eh_frame)
    if(0 == xcd_elf_interface_dwarf_step(self->interface, step_pc, regs, finished)) return 0;

    //create GNU interface (only once)
    if(NULL == self->gnu_interface && 0 == self->gnu_interface_created)
    {
        self->gnu_interface_created = 1;
        self->gnu_interface = xcd_elf_interface_gnu_create(self->interface);
    }

    //try DWARF (.debug_frame and .eh_frame) in GNU interface
    if(NULL != self->gnu_interface)
        if(0 == xcd_elf_interface_dwarf_step(self->gnu_interface, step_pc, regs, finished)) return 0;

    //try .ARM.exidx
#ifdef __arm__
    if(0 == xcd_elf_interface_arm_exidx_step(self->interface, step_pc, regs, finished)) return 0;
#endif

#if XCD_ELF_DEBUG
    XCD_LOG_ERROR("ELF: step FAILED, rel_pc=%"PRIxPTR", step_pc=%"PRIxPTR, rel_pc, step_pc);
#endif
    return XCC_ERRNO_MISSING;
}

int xcd_elf_get_function_info(xcd_elf_t *self, uintptr_t addr, char **name, size_t *name_offset)
{
    int r;

    //try ELF interface
    if(0 == (r = xcd_elf_interface_get_function_info(self->interface, addr, name, name_offset))) return 0;

    //create GNU interface (only once)
    if(NULL == self->gnu_interface && 0 == self->gnu_interface_created)
    {
        self->gnu_interface_created = 1;
        self->gnu_interface = xcd_elf_interface_gnu_create(self->interface);
    }
    
    //try GNU interface
    if(NULL != self->gnu_interface)
        if(0 == (r = xcd_elf_interface_get_function_info(self->gnu_interface, addr, name, name_offset))) return 0;

    return r;
}

int xcd_elf_get_symbol_addr(xcd_elf_t *self, const char *name, uintptr_t *addr)
{
    return xcd_elf_interface_get_symbol_addr(self->interface, name, addr);
}

int xcd_elf_get_build_id(xcd_elf_t *self, uint8_t *build_id, size_t build_id_len, size_t *build_id_len_ret)
{
    return xcd_elf_interface_get_build_id(self->interface, build_id, build_id_len, build_id_len_ret);
}

char *xcd_elf_get_so_name(xcd_elf_t *self)
{
    return xcd_elf_interface_get_so_name(self->interface);
}
