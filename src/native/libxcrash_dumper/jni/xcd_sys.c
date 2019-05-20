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
#include <sys/system_properties.h>
#include "xcc_version.h"
#include "xcc_util.h"
#include "xcd_sys.h"
#include "xcd_util.h"

static int xcd_sys_record_time(int log_fd, const char *title, uint64_t time)
{
    time_t      sec = (time_t)(time / 1000000);
    suseconds_t usec = (time_t)(time % 1000000);
    struct tm   tm;

    if(NULL == localtime_r(&sec, &tm))
        return xcc_util_write_format(log_fd, "%s: 'unknown (%"PRIu64")'\n", title, time);
    
    return xcc_util_write_format(log_fd,
                                 "%s: '%04d-%02d-%02dT%02d:%02d:%02d.%03ld%c%02ld%02ld'\n",
                                 title,
                                 tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
                                 tm.tm_hour, tm.tm_min, tm.tm_sec, usec / 1000,
                                 tm.tm_gmtoff < 0 ? '-' : '+', labs(tm.tm_gmtoff / 3600), labs(tm.tm_gmtoff % 3600));
}

static int xcd_sys_record_file_line(int log_fd, const char *title, const char *path)
{
    FILE *f = NULL;
    char  buf[256];
    char *p = "unknown";
    
    if(NULL == (f = fopen(path, "r"))) goto end;
    if(NULL == fgets(buf, sizeof(buf), f)) goto end;
    p = xcc_util_trim(buf);

 end:
    if(NULL != f) fclose(f);
    return xcc_util_write_format(log_fd, "%s: '%s'\n", title, p);
}

static int xcd_sys_record_cpu(int log_fd)
{
    int r;
    
    if(0 != (r = xcd_sys_record_file_line(log_fd, "CPU loadavg", "/proc/loadavg"))) return r;
    if(0 != (r = xcd_sys_record_file_line(log_fd, "CPU online",  "/sys/devices/system/cpu/online"))) return r;
    if(0 != (r = xcd_sys_record_file_line(log_fd, "CPU offline", "/sys/devices/system/cpu/offline"))) return r;

    return 0;
}

static int xcd_sys_record_kernel_version(int log_fd)
{
    char buf[256];
    
    xcc_util_get_kernel_version(buf, sizeof(buf));

    return xcc_util_write_format(log_fd, "Kernel version: '%s'\n", buf);
}

static int xcd_sys_record_system_mem(int log_fd)
{
    FILE   *f = NULL;
    char    line[256];
    size_t  mtotal = 0;
    size_t  mfree = 0;
    size_t  mbuffers = 0;
    size_t  mcached = 0;
    size_t  mtotal_found = 0;
    size_t  mfree_found = 0;
    size_t  mbuffers_found = 0;
    size_t  mcached_found = 0;
    int     r;

    if(NULL == (f = fopen("/proc/meminfo", "r"))) goto end;
    
    while(NULL != fgets(line, sizeof(line), f))
    {
        if(0 == memcmp(line, "MemTotal:", 9))
        {
            if(0 == sscanf(line, "MemTotal: %zu kB", &mtotal)) goto end;
            mtotal_found = 1;
        }
        else if(0 == memcmp(line, "MemFree:", 8))
        {
            if(0 == sscanf(line, "MemFree: %zu kB", &mfree)) goto end;
            mfree_found = 1;
        }
        else if(0 == memcmp(line, "Buffers:", 8))
        {
            if(0 == sscanf(line, "Buffers: %zu kB", &mbuffers)) goto end;
            mbuffers_found = 1;
        }
        else if(0 == memcmp(line, "Cached:", 7))
        {
            if(0 == sscanf(line, "Cached: %zu kB", &mcached)) goto end;
            mcached_found = 1;
        }

        if(1 == mtotal_found && 1 == mfree_found && 1 == mbuffers_found && 1 == mcached_found) break;
    }
    
 end:
    if(NULL != f) fclose(f);
    if(0 != (r = xcc_util_write_format(log_fd, "System memory total: '%zu kB'\n", mtotal))) return r;
    if(0 != (r = xcc_util_write_format(log_fd, "System memory used: '%zu kB'\n", mtotal - mfree - mbuffers - mcached))) return r;
    return 0;
}

int xcd_sys_record(int log_fd, uint64_t start_time, uint64_t crash_time,
                   const char *app_id, const char *app_version,
                   xcc_util_build_prop_t *props, size_t nthds)
{
    int r;
    
    if(0 != (r = xcc_util_write_str(log_fd, XCC_UTIL_TOMB_HEAD))) return r;
    
    if(0 != (r = xcc_util_write_format(log_fd, "Tombstone maker: '%s'\n",    XCC_VERSION_STR))) return r;
    if(0 != (r = xcc_util_write_format(log_fd, "Crash type: '%s'\n",         XCC_UTIL_CRASH_TYPE))) return r;
    if(0 != (r = xcd_sys_record_time  (log_fd, "Start time",                 start_time))) return r;
    if(0 != (r = xcd_sys_record_time  (log_fd, "Crash time",                 crash_time))) return r;
    if(0 != (r = xcc_util_write_format(log_fd, "App ID: '%s'\n",             app_id))) return r;
    if(0 != (r = xcc_util_write_format(log_fd, "App version: '%s'\n",        app_version))) return r;
    
    if(0 != (r = xcd_sys_record_cpu(log_fd))) return r;
    if(0 != (r = xcd_sys_record_system_mem(log_fd))) return r;
    
    if(0 != (r = xcc_util_write_format(log_fd, "Number of threads: '%zu'\n", nthds))) return r;
    if(0 != (r = xcc_util_write_format(log_fd, "Rooted: '%s'\n",             xcc_util_is_root() ? "Yes" : "No"))) return r;
    if(0 != (r = xcc_util_write_format(log_fd, "API level: '%d'\n",          props->api_level))) return r;
    if(0 != (r = xcc_util_write_format(log_fd, "OS version: '%s'\n",         props->os_version))) return r;
    
    if(0 != (r = xcd_sys_record_kernel_version(log_fd))) return r;
    
    if(0 != (r = xcc_util_write_format(log_fd, "ABI list: '%s'\n",           props->abi_list))) return r;
    if(0 != (r = xcc_util_write_format(log_fd, "Manufacturer: '%s'\n",       props->manufacturer))) return r;
    if(0 != (r = xcc_util_write_format(log_fd, "Brand: '%s'\n",              props->brand))) return r;
    if(0 != (r = xcc_util_write_format(log_fd, "Model: '%s'\n",              props->model))) return r;
    if(0 != (r = xcc_util_write_format(log_fd, "Build fingerprint: '%s'\n",  props->build_fingerprint))) return r;
    if(0 != (r = xcc_util_write_format(log_fd, "Revision: '%s'\n",           props->revision))) return r;
    if(0 != (r = xcc_util_write_format(log_fd, "ABI: '%s'\n",                XCC_UTIL_ABI_STRING))) return r;

    return 0;
}
