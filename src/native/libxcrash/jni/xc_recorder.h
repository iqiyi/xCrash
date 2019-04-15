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

#ifndef XC_RECORDER_H
#define XC_RECORDER_H 1

#include <stdint.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct xc_recorder xc_recorder_t;

int xcd_recorder_create(xc_recorder_t **self, uint64_t start_time, const char *app_version,
                        const char *log_dir, const char *log_prefix, const char *log_suffix,
                        size_t log_cnt_max);
void xcd_recorder_destroy(xc_recorder_t **self);

int xc_recorder_create_log(xc_recorder_t *self);
int xc_recorder_create_log_ok(xc_recorder_t *self);
char *xc_recorder_get_log_pathname(xc_recorder_t *self);

int xc_recorder_log_err(xc_recorder_t *self, const char *msg, int errnum);
int xc_recorder_log_err_msg(xc_recorder_t *self, const char *msg, int errnum, const char *errmsg);

int xc_recorder_open(xc_recorder_t *self, int *fd);
void xc_recorder_close(xc_recorder_t *self, int fd);

int xc_recorder_check_backtrace_valid(xc_recorder_t *self);

#ifdef __cplusplus
}
#endif

#endif
