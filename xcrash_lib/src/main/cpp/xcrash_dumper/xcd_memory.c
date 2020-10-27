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
#include <sys/mman.h>
#include "xcc_errno.h"
#include "xcd_map.h"
#include "xcd_memory.h"
#include "xcd_memory_file.h"
#include "xcd_memory_buf.h"
#include "xcd_memory_remote.h"

extern const xcd_memory_handlers_t xcd_memory_buf_handlers;
extern const xcd_memory_handlers_t xcd_memory_file_handlers;
extern const xcd_memory_handlers_t xcd_memory_remote_handlers;

struct xcd_memory
{
    void                        *obj;
    const xcd_memory_handlers_t *handlers;
};

int xcd_memory_create(xcd_memory_t **self, void *map_obj, pid_t pid, void *maps_obj)
{
    xcd_map_t  *map  = (xcd_map_t *)map_obj;
    xcd_maps_t *maps = (xcd_maps_t *)maps_obj;
    
    if(map->end <= map->start) return XCC_ERRNO_INVAL;
    if(map->flags & XCD_MAP_PORT_DEVICE) return XCC_ERRNO_DEV;
    
    if(NULL == (*self = malloc(sizeof(xcd_memory_t)))) return XCC_ERRNO_NOMEM;
    
    //try memory from file
    (*self)->handlers = &xcd_memory_file_handlers;
    if(0 == xcd_memory_file_create(&((*self)->obj), *self, map, maps)) return 0;

    //try memory from remote ptrace
    //Sometimes, data only exists in the remote process's memory such as vdso data on x86.
    if(!(map->flags & PROT_READ)) return XCC_ERRNO_PERM;
    (*self)->handlers = &xcd_memory_remote_handlers;
    if(0 == xcd_memory_remote_create(&((*self)->obj), map, pid)) return 0;
    (void)pid;

    free(*self);
    return XCC_ERRNO_MEM;
}

//for ELF header info unzipped from .gnu_debugdata in the local memory
int xcd_memory_create_from_buf(xcd_memory_t **self, uint8_t *buf, size_t len)
{
    if(NULL == (*self = malloc(sizeof(xcd_memory_t)))) return XCC_ERRNO_NOMEM;
    (*self)->handlers = &xcd_memory_buf_handlers;
    if(0 == xcd_memory_buf_create(&((*self)->obj), buf, len)) return 0;

    free(*self);
    return XCC_ERRNO_MEM;
}

void xcd_memory_destroy(xcd_memory_t **self)
{
    (*self)->handlers->destroy(&((*self)->obj));
    free(*self);
    *self = NULL;
}

size_t xcd_memory_read(xcd_memory_t *self, uintptr_t addr, void *dst, size_t size)
{
    return self->handlers->read(self->obj, addr, dst, size);
}

int xcd_memory_read_fully(xcd_memory_t *self, uintptr_t addr, void* dst, size_t size)
{
    size_t rc = self->handlers->read(self->obj, addr, dst, size);
    return rc == size ? 0 : XCC_ERRNO_MISSING;
}

int xcd_memory_read_string(xcd_memory_t *self, uintptr_t addr, char *dst, size_t size, size_t max_read)
{
    char   value;
    size_t i = 0;
    int    r;
    
    while(i < size && i < max_read)
    {
        if(0 != (r = xcd_memory_read_fully(self, addr, &value, sizeof(value)))) return r;
        dst[i] = value;
        if('\0' == value) return 0;
        addr++;
        i++;
    }
    return XCC_ERRNO_NOSPACE;
}

int xcd_memory_read_uleb128(xcd_memory_t *self, uintptr_t addr, uint64_t *dst, size_t *size)
{
    uint64_t cur_value = 0;
    uint64_t shift = 0;
    uint8_t  byte;
    int      r;
    
    if(NULL != size) *size = 0;
    
    do
    {
        if(0 != (r = xcd_memory_read_fully(self, addr, &byte, 1))) return r;
        addr += 1;
        if(NULL != size) *size += 1;
        cur_value += ((uint64_t)(byte & 0x7f) << shift);
        shift += 7;
    }while(byte & 0x80);
    
    *dst = cur_value;
    return 0;
}

int xcd_memory_read_sleb128(xcd_memory_t *self, uintptr_t addr, int64_t *dst, size_t *size)
{
    uint64_t cur_value = 0;
    uint64_t shift = 0;
    uint8_t  byte;
    int      r;

    if(NULL != size) *size = 0;
    
    do
    {
        if(0 != (r = xcd_memory_read_fully(self, addr, &byte, 1))) return r;
        addr += 1;
        if(NULL != size) *size += 1;
        cur_value += ((uint64_t)(byte & 0x7f) << shift);
        shift += 7;
    }while(byte & 0x80);

    if(byte & 0x40)
        cur_value |= ((uint64_t)(-1) << shift);
    
    *dst = (int64_t)cur_value;
    return 0;    
}
