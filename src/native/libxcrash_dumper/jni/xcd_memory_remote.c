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
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "xcc_errno.h"
#include "xcc_util.h"
#include "xcd_map.h"
#include "xcd_memory.h"
#include "xcd_memory_remote.h"
#include "xcd_util.h"
#include "xcd_log.h"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpadded"
struct xcd_memory_remote
{
    pid_t     pid;
    uintptr_t start;
    size_t    length;
};
#pragma clang diagnostic pop

int xcd_memory_remote_create(void **obj, xcd_map_t *map, pid_t pid)
{
    xcd_memory_remote_t **self = (xcd_memory_remote_t **)obj;
    
    if(NULL == (*self = malloc(sizeof(xcd_memory_remote_t)))) return XCC_ERRNO_NOMEM;
    (*self)->pid = pid;
    (*self)->start = map->start;
    (*self)->length = (size_t)(map->end - map->start);

    return 0;
}

void xcd_memory_remote_destroy(void **obj)
{
    xcd_memory_remote_t **self = (xcd_memory_remote_t **)obj;
    
    free(*self);
    *self = NULL;
}

size_t xcd_memory_remote_read(void *obj, uintptr_t addr, void *dst, size_t size)
{
    xcd_memory_remote_t *self = (xcd_memory_remote_t *)obj;

    if((size_t)addr >= self->length) return 0;

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wgnu-statement-expression"
    size_t read_length = XCC_UTIL_MIN(size, self->length - addr);
#pragma clang diagnostic pop
    
    uint64_t read_addr;
    if(__builtin_add_overflow(self->start, addr, &read_addr)) return 0;
    
    return xcd_util_ptrace_read(self->pid, (uintptr_t)read_addr, dst, read_length);
}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-variable-declarations"
const xcd_memory_handlers_t xcd_memory_remote_handlers = {
    xcd_memory_remote_destroy,
    xcd_memory_remote_read
};
#pragma clang diagnostic pop
