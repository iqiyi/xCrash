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

#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <sys/mman.h>
#include "xcc_errno.h"
#include "xcc_util.h"
#include "xcd_map.h"
#include "xcd_util.h"
#include "xcd_log.h"

int xcd_map_init(xcd_map_t *self, uintptr_t start, uintptr_t end, size_t offset,
                 const char * flags, const char *name)
{
    self->start  = start;
    self->end    = end;
    self->offset = offset;
    
    self->flags  = PROT_NONE;
    if(flags[0] == 'r') self->flags |= PROT_READ;
    if(flags[1] == 'w') self->flags |= PROT_WRITE;
    if(flags[2] == 'x') self->flags |= PROT_EXEC;
    
    if(NULL == name || '\0' == name[0])
    {
        self->name = NULL;
    }
    else
    {
        if(0 == strncmp(name, "/dev/", 5) && 0 != strncmp(name + 5, "ashmem/", 7))
            self->flags |= XCD_MAP_PORT_DEVICE;
        
        if(NULL == (self->name = strdup(name))) return XCC_ERRNO_NOMEM;
    }

    self->elf = NULL;
    self->elf_loaded = 0;
    self->elf_offset = 0;
    self->elf_start_offset = 0;

    return 0;
}

void xcd_map_uninit(xcd_map_t *self)
{
    free(self->name);
    self->name = NULL;
}

xcd_elf_t *xcd_map_get_elf(xcd_map_t *self, pid_t pid, void *maps_obj)
{
    xcd_memory_t *memory = NULL;
    xcd_elf_t    *elf = NULL;

    if(NULL == self->elf && 0 == self->elf_loaded)
    {
        self->elf_loaded = 1;
        
        if(0 != xcd_memory_create(&memory, self, pid, maps_obj)) return NULL;

        if(0 != xcd_elf_create(&elf, pid, memory)) return NULL;
        
        self->elf = elf;
    }

    return self->elf;
}

uintptr_t xcd_map_get_rel_pc(xcd_map_t *self, uintptr_t pc, pid_t pid, void *maps_obj)
{
    xcd_elf_t *elf = xcd_map_get_elf(self, pid, maps_obj);
    uintptr_t load_bias = (NULL == elf ? 0 : xcd_elf_get_load_bias(elf));
    
    return pc - self->start + load_bias + self->elf_offset;
}

uintptr_t xcd_map_get_abs_pc(xcd_map_t *self, uintptr_t pc, pid_t pid, void *maps_obj)
{
    xcd_elf_t *elf = xcd_map_get_elf(self, pid, maps_obj);
    uintptr_t load_bias = (NULL == elf ? 0 : xcd_elf_get_load_bias(elf));
    
    return self->start + pc - load_bias - self->elf_offset;
}
