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
#include "xcd_memory_buf.h"
#include "xcd_util.h"

struct xcd_memory_buf
{
    uint8_t *buf;
    size_t   len;
};

int xcd_memory_buf_create(void **obj, uint8_t *buf, size_t len)
{
    xcd_memory_buf_t **self = (xcd_memory_buf_t **)obj;
    
    if(NULL == (*self = malloc(sizeof(xcd_memory_buf_t)))) return XCC_ERRNO_NOMEM;
    (*self)->buf = buf;
    (*self)->len = len;

    return 0;
}

void xcd_memory_buf_destroy(void **obj)
{
    xcd_memory_buf_t **self = (xcd_memory_buf_t **)obj;

    free((*self)->buf);
    free(*self);
    *self = NULL;
}

size_t xcd_memory_buf_read(void *obj, uintptr_t addr, void *dst, size_t size)
{
    xcd_memory_buf_t *self = (xcd_memory_buf_t *)obj;

    if((size_t)addr >= self->len) return 0;

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wgnu-statement-expression"
    size_t read_length = XCC_UTIL_MIN(size, self->len - addr);
#pragma clang diagnostic pop
    
    memcpy(dst, self->buf + addr, read_length);
    return read_length;
}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-variable-declarations"
const xcd_memory_handlers_t xcd_memory_buf_handlers = {
    xcd_memory_buf_destroy,
    xcd_memory_buf_read
};
#pragma clang diagnostic pop
