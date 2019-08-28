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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "xcc_util.h"
#include "xcd_sys.h"

int xcd_sys_record(int fd,
                   long time_zone,
                   uint64_t start_time,
                   uint64_t crash_time,
                   const char *app_id,
                   const char *app_version,
                   int api_level,
                   const char *os_version,
                   const char *kernel_version,
                   const char *abi_list,
                   const char *manufacturer,
                   const char *brand,
                   const char *model,
                   const char *build_fingerprint)
{
    char buf[1024];
    xcc_util_get_dump_header(buf, sizeof(buf),
                             XCC_UTIL_CRASH_TYPE_NATIVE,
                             time_zone,
                             start_time,
                             crash_time,
                             app_id,
                             app_version,
                             api_level,
                             os_version,
                             kernel_version,
                             abi_list,
                             manufacturer,
                             brand,
                             model,
                             build_fingerprint);
    return xcc_util_write_str(fd, buf);
}
