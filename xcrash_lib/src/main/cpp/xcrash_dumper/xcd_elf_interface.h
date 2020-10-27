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

#ifndef XCD_ELF_INTERFACE_H
#define XCD_ELF_INTERFACE_H 1

#include <stdint.h>
#include <sys/types.h>
#include "xcd_memory.h"
#include "xcd_regs.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct xcd_elf_interface xcd_elf_interface_t;

int xcd_elf_interface_create(xcd_elf_interface_t **self, pid_t pid, xcd_memory_t *memory, uintptr_t *load_bias);

xcd_elf_interface_t *xcd_elf_interface_gnu_create(xcd_elf_interface_t *self);

int xcd_elf_interface_dwarf_step(xcd_elf_interface_t *self, uintptr_t step_pc, xcd_regs_t *regs, int *finished);
#ifdef __arm__
int xcd_elf_interface_arm_exidx_step(xcd_elf_interface_t *self, uintptr_t step_pc, xcd_regs_t *regs, int *finished);
#endif

int xcd_elf_interface_get_function_info(xcd_elf_interface_t *self, uintptr_t addr, char **name, size_t *name_offset);
int xcd_elf_interface_get_symbol_addr(xcd_elf_interface_t *self, const char *name, uintptr_t *addr);

int xcd_elf_interface_get_build_id(xcd_elf_interface_t *self, uint8_t *build_id, size_t build_id_len, size_t *build_id_len_ret);
char *xcd_elf_interface_get_so_name(xcd_elf_interface_t *self);

#ifdef __cplusplus
}
#endif

#endif
