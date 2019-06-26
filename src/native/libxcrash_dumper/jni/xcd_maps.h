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

#ifndef XCD_MAPS_H
#define XCD_MAPS_H 1

#include <stdint.h>
#include <sys/types.h>
#include "xcd_map.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct xcd_maps xcd_maps_t;

int xcd_maps_create(xcd_maps_t **self, pid_t pid);
void xcd_maps_destroy(xcd_maps_t **self);

xcd_map_t *xcd_maps_find_map(xcd_maps_t *self, uintptr_t pc);
xcd_map_t *xcd_maps_get_prev_map(xcd_maps_t *self, xcd_map_t *cur_map);

uintptr_t xcd_maps_find_pc(xcd_maps_t *self, const char *pathname, const char *symbol);

int xcd_maps_record(xcd_maps_t *self, int log_fd);

#ifdef __cplusplus
}
#endif

#endif
