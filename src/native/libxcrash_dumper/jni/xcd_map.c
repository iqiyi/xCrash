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
#include "xcd_recorder.h"
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
    
    if(NULL == name)
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
    self->elf_offset = 0;
    self->elf_loaded = 0;

    return 0;
}

void xcd_map_uninit(xcd_map_t *self)
{
    free(self->name);
    self->name = NULL;
}

int xcd_map_record(xcd_map_t *self, xcd_recorder_t *recorder)
{
    uintptr_t  load_bias = 0;
    char       load_bias_buf[64] = "\0";
    uint8_t    build_id[64];
    size_t     build_id_len = 0;
    char       build_id_buf[64 * 2 + 64] = "\0";
    size_t     offset = 0;
    size_t     i;

    if(NULL != self->elf)
    {
        //get load_bias
        if(0 != (load_bias = xcd_elf_get_load_bias(self->elf)))
            snprintf(load_bias_buf, sizeof(load_bias_buf), " (load base 0x%"PRIxPTR")", load_bias);

        //get build ID
        if(0 == xcd_elf_get_build_id(self->elf, build_id, sizeof(build_id), &build_id_len))
        {
            offset = snprintf(build_id_buf + offset, sizeof(build_id_buf) - offset, "%s", " (BuildId: ");
            for(i = 0; i < build_id_len; i++)
                offset += snprintf(build_id_buf + offset, sizeof(build_id_buf) - offset, "%02x", build_id[i]);
            snprintf(build_id_buf + offset, sizeof(build_id_buf) - offset, "%s", ")");
        }
    }
    
    return xcd_recorder_print(recorder,
                              "    %0"XCC_UTIL_FMT_ADDR"-%0"XCC_UTIL_FMT_ADDR" %c%c%c  %"XCC_UTIL_FMT_ADDR"  %"XCC_UTIL_FMT_ADDR"  %s%s%s\n",
                              self->start, self->end - 1,
                              self->flags & PROT_READ ? 'r' : '-',
                              self->flags & PROT_WRITE ? 'w' : '-',
                              self->flags & PROT_EXEC ? 'x' : '-',
                              self->offset,
                              self->end - self->start,
                              (NULL == self->name ? "" : self->name),
                              load_bias_buf,
                              build_id_buf);
}

xcd_elf_t *xcd_map_get_elf(xcd_map_t *self, pid_t pid)
{
    xcd_memory_t *memory = NULL;
    xcd_elf_t    *elf = NULL;

    if(NULL == self->elf && 0 == self->elf_loaded)
    {
        self->elf_loaded = 1;
        
        if(0 != xcd_memory_create(&memory, self, pid)) return NULL;

        if(0 != xcd_elf_create(&elf, pid, memory)) return NULL;
        
        self->elf = elf;
    }

    return self->elf;
}

uintptr_t xcd_map_get_rel_pc(xcd_map_t *self, uintptr_t pc, pid_t pid)
{
    xcd_elf_t *elf = xcd_map_get_elf(self, pid);
    uintptr_t load_bias = (NULL == elf ? 0 : xcd_elf_get_load_bias(elf));
    
    uintptr_t load_base = self->start - self->elf_offset;

    return pc - (load_base - load_bias);
}
