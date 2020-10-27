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

#ifndef XCD_MEMORY_H
#define XCD_MEMORY_H 1

#include <stdint.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct xcd_memory xcd_memory_t;

typedef struct
{
    void (*destroy)(void **self);
    size_t (*read)(void *self, uintptr_t addr, void *dst, size_t size);
} xcd_memory_handlers_t;

int xcd_memory_create(xcd_memory_t **self, void *map_obj, pid_t pid, void *maps_obj);
int xcd_memory_create_from_buf(xcd_memory_t **self, uint8_t *buf, size_t len);
void xcd_memory_destroy(xcd_memory_t **self);

size_t xcd_memory_read(xcd_memory_t *self, uintptr_t addr, void *dst, size_t size);
int xcd_memory_read_fully(xcd_memory_t *self, uintptr_t addr, void* dst, size_t size);
int xcd_memory_read_string(xcd_memory_t *self, uintptr_t addr, char *dst, size_t size, size_t max_read);
int xcd_memory_read_uleb128(xcd_memory_t *self, uintptr_t addr, uint64_t *dst, size_t *size);
int xcd_memory_read_sleb128(xcd_memory_t *self, uintptr_t addr, int64_t *dst, size_t *size);

#ifdef __cplusplus
}
#endif

#endif
