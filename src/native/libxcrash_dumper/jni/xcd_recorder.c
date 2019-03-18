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
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "xcc_errno.h"
#include "xcc_util.h"
#include "xcd_recorder.h"
#include "xcd_util.h"
#include "xcd_log.h"

struct xcd_recorder
{
    int     fd;
    char   *buf;
    size_t  buf_len;
};

int xcd_recorder_create(xcd_recorder_t **self, const char *pathname)
{
    int r;

    if(NULL == (*self = malloc(sizeof(xcd_recorder_t)))) return XCC_ERRNO_NOMEM;
    (*self)->fd      = -1;
    (*self)->buf     = NULL;
    (*self)->buf_len = 1024;
    
    if(0 > ((*self)->fd = open(pathname, O_WRONLY | O_TRUNC)))
    {
        r = XCC_ERRNO_SYS;
        goto err;
    }
    if(NULL == ((*self)->buf = malloc((*self)->buf_len)))
    {
        r = XCC_ERRNO_NOMEM;
        goto err;
    }
    return 0;

 err:
    if((*self)->fd >= 0) close((*self)->fd);
    if(NULL != (*self)->buf) free((*self)->buf);
    free(*self);
    *self = NULL;
    XCD_LOG_ERROR("RECORDER: create recorder failed, errno=%d", r);
    return r;
}

void xcd_recorder_destroy(xcd_recorder_t **self)
{
    close((*self)->fd);
    free((*self)->buf);
    free(*self);
    *self = NULL;
}

int xcd_recorder_get_fd(xcd_recorder_t *self)
{
    if(NULL == self) return -1;
    return self->fd;
}

int xcd_recorder_write(xcd_recorder_t *self, const char *str)
{
    return xcc_util_write_str(self->fd, str);
}

int xcd_recorder_print(xcd_recorder_t *self, const char *format, ...)
{
    va_list ap;
    int r;

    va_start(ap, format);
    r = xcd_recorder_vprint(self, format, ap);
    va_end(ap);
    return r;
}

int xcd_recorder_vprint(xcd_recorder_t *self, const char *format, va_list ap)
{
    int     len;
    char   *buf_new;
    size_t  buf_len_new;
    
    len = vsnprintf(self->buf, self->buf_len, format, ap);
    if(len < 0) return XCC_ERRNO_SYS;
    
    if((size_t)len >= self->buf_len)
    {
        buf_len_new = (size_t)len + 1;
        if(0 != buf_len_new % 128) buf_len_new += (128 - buf_len_new % 128);
        
        if(NULL == (buf_new = realloc(self->buf, buf_len_new))) return XCC_ERRNO_NOMEM;
        
        self->buf     = buf_new;
        self->buf_len = buf_len_new;

        len = vsnprintf(self->buf, self->buf_len, format, ap);
        if(len < 0) return XCC_ERRNO_SYS;
        if((size_t)len >= self->buf_len) return XCC_ERRNO_UNKNOWN;
    }

    return xcc_util_write(self->fd, self->buf, (size_t)len);
}
