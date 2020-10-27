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

#include <ctype.h>
#include <time.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <dlfcn.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <sys/wait.h>
#include "xcc_errno.h"
#include "xcc_fmt.h"
#include "xcc_util.h"
#include "xc_util.h"

char *xc_util_strdupcat(const char *s1, const char *s2)
{
    size_t s1_len, s2_len;
    char *s;

    if(NULL == s1 || NULL == s2) return NULL;

    s1_len = strlen(s1);
    s2_len = strlen(s2);

    if(NULL == (s = malloc(s1_len + s2_len + 1))) return NULL;
    memcpy(s, s1, s1_len);
    memcpy(s + s1_len, s2, s2_len + 1);

    return s;
}

int xc_util_mkdirs(const char *dir)
{
    size_t  len;
    char    buf[PATH_MAX];
    char   *p;

    //check or try create dir directly
    errno = 0;
    if(0 == mkdir(dir, S_IRWXU) || EEXIST == errno) return 0;

    //trycreate dir recursively...

    len = strlen(dir);
    if(0 == len) return XCC_ERRNO_INVAL;
    if('/' != dir[0]) return XCC_ERRNO_INVAL;
    
    memcpy(buf, dir, len + 1);
    if(buf[len - 1] == '/') buf[len - 1] = '\0';
    
    for(p = buf + 1; *p; p++)
    {
        if(*p == '/')
        {
            *p = '\0';
            errno = 0;
            if(0 != mkdir(buf, S_IRWXU) && EEXIST != errno) return errno;
            *p = '/';
        }
    }
    errno = 0;
    if(0 != mkdir(buf, S_IRWXU) && EEXIST != errno) return errno;
    return 0;
}

void xc_util_get_kernel_version(char *buf, size_t len)
{
    struct utsname uts;

    if(0 != uname(&uts))
    {
        strncpy(buf, "unknown", len);
        buf[len - 1] = '\0';
        return;
    }

    snprintf(buf, len, "%s version %s %s (%s)", uts.sysname, uts.release, uts.version, uts.machine);
}
