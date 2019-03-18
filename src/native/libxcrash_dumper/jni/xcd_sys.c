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

static int xcd_sys_record_time(xcd_recorder_t *recorder, const char *title, uint64_t time)
{
    time_t      sec = (time_t)(time / 1000000);
    suseconds_t usec = (time_t)(time % 1000000);
    struct tm   tm;

    if(NULL == localtime_r(&sec, &tm))
        return xcd_recorder_print(recorder, "%s: 'unknown (%"PRIu64")'\n", title, time);
    
    return xcd_recorder_print(recorder, "%s: '%04d-%02d-%02dT%02d:%02d:%02d.%03ld%c%02ld%02ld'\n", title,
                              tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
                              tm.tm_hour, tm.tm_min, tm.tm_sec, usec / 1000,
                              tm.tm_gmtoff < 0 ? '-' : '+', labs(tm.tm_gmtoff / 3600), labs(tm.tm_gmtoff % 3600));
}

static int xcd_sys_record_file_line(xcd_recorder_t *recorder, const char *title, const char *path)
{
    FILE *f = NULL;
    char  buf[256];
    char *p = "unknown";
    
    if(NULL == (f = fopen(path, "r"))) goto end;
    if(NULL == fgets(buf, sizeof(buf), f)) goto end;
    p = xcc_util_trim(buf);

 end:
    if(NULL != f) fclose(f);
    return xcd_recorder_print(recorder, "%s: '%s'\n", title, p);
}

static int xcd_sys_record_cpu(xcd_recorder_t *recorder)
{
    int r;
    
    if(0 != (r = xcd_sys_record_file_line(recorder, "CPU loadavg", "/proc/loadavg"))) return r;
    if(0 != (r = xcd_sys_record_file_line(recorder, "CPU online",  "/sys/devices/system/cpu/online"))) return r;
    if(0 != (r = xcd_sys_record_file_line(recorder, "CPU offline", "/sys/devices/system/cpu/offline"))) return r;

    return 0;
}

static int xcd_sys_record_system_mem(xcd_recorder_t *recorder)
{
    FILE   *f = NULL;
    char    line[256];
    size_t  mtotal = 0;
    size_t  mfree = 0;
    size_t  mbuffers = 0;
    size_t  mcached = 0;
    int     r;

    if(NULL == (f = fopen("/proc/meminfo", "r"))) goto end;
    
    while(NULL != fgets(line, sizeof(line), f))
    {
        if(0 == memcmp(line, "MemTotal:", 9))
        {
            if(0 == sscanf(line, "MemTotal: %zu kB", &mtotal)) goto end;
        }
        else if(0 == memcmp(line, "MemFree:", 8))
        {
            if(0 == sscanf(line, "MemFree: %zu kB", &mfree)) goto end;
        }
        else if(0 == memcmp(line, "Buffers:", 8))
        {
            if(0 == sscanf(line, "Buffers: %zu kB", &mbuffers)) goto end;
        }
        else if(0 == memcmp(line, "Cached:", 7))
        {
            if(0 == sscanf(line, "Cached: %zu kB", &mcached)) goto end;
        }
    }
    
 end:
    if(NULL != f) fclose(f);
    if(0 != (r = xcd_recorder_print(recorder, "System memory total: '%zu kB'\n", mtotal))) return r;
    if(0 != (r = xcd_recorder_print(recorder, "System memory used: '%zu kB'\n", mtotal - mfree - mbuffers - mcached))) return r;
    return 0;
}

static int xcd_sys_record_process_mem(xcd_recorder_t *recorder, pid_t pid)
{
    FILE   *f = NULL;
    char    buf[64];
    char    line[256];
    size_t  total_rss = 0;
    size_t  total_pss = 0;
    size_t  v;
    int     r;

    snprintf(buf, sizeof(buf), "/proc/%d/smaps", pid);
    
    if(NULL == (f = fopen(buf, "r"))) goto end;
    
    while(NULL != fgets(line, sizeof(line), f))
    {
        if(0 == memcmp(line, "Rss:", 4))
        {
            if(0 == sscanf(line, "Rss: %zu kB", &v)) goto end;
            total_rss += v;
        }
        else if(0 == memcmp(line, "Pss:", 4))
        {
            if(0 == sscanf(line, "Pss: %zu kB", &v)) goto end;
            total_pss += v;
        }
    }
    
 end:
    if(NULL != f) fclose(f);
    if(0 != (r = xcd_recorder_print(recorder, "Process memory RSS: '%zu kB'\n", total_rss))) return r;
    if(0 != (r = xcd_recorder_print(recorder, "Process memory PSS: '%zu kB'\n", total_pss))) return r;
    return 0;
}

int xcd_sys_record(xcd_recorder_t *recorder, pid_t pid, uint64_t start_time, uint64_t crash_time,
                   const char *app_id, const char *app_version, xcc_util_build_prop_t *props, size_t nthds)
{
    int r;
    
    if(0 != (r = xcd_recorder_write(recorder, XCC_UTIL_TOMB_HEAD))) return r;
    
    if(0 != (r = xcd_recorder_print (recorder, "Tombstone maker: '%s'\n",    XCC_VERSION_STR))) return r;
    if(0 != (r = xcd_recorder_print (recorder, "Crash type: '%s'\n",         XCC_UTIL_CRASH_TYPE))) return r;
    if(0 != (r = xcd_sys_record_time(recorder, "Start time",                 start_time))) return r;
    if(0 != (r = xcd_sys_record_time(recorder, "Crash time",                 crash_time))) return r;
    if(0 != (r = xcd_recorder_print (recorder, "App ID: '%s'\n",             app_id))) return r;
    if(0 != (r = xcd_recorder_print (recorder, "App version: '%s'\n",        app_version))) return r;
    
    if(0 != (r = xcd_sys_record_cpu(recorder))) return r;
    if(0 != (r = xcd_sys_record_system_mem(recorder))) return r;
    if(0 != (r = xcd_sys_record_process_mem(recorder, pid))) return r;
    
    if(0 != (r = xcd_recorder_print(recorder, "Number of threads: '%zu'\n", nthds))) return r;
    if(0 != (r = xcd_recorder_print(recorder, "Rooted: '%s'\n",             xcc_util_is_root() ? "Yes" : "No"))) return r;
    if(0 != (r = xcd_recorder_print(recorder, "API level: '%d'\n",          props->api_level))) return r;
    if(0 != (r = xcd_recorder_print(recorder, "OS version: '%s'\n",         props->os_version))) return r;
    if(0 != (r = xcd_recorder_print(recorder, "ABI list: '%s'\n",           props->abi_list))) return r;
    if(0 != (r = xcd_recorder_print(recorder, "Manufacturer: '%s'\n",       props->manufacturer))) return r;
    if(0 != (r = xcd_recorder_print(recorder, "Brand: '%s'\n",              props->brand))) return r;
    if(0 != (r = xcd_recorder_print(recorder, "Model: '%s'\n",              props->model))) return r;
    if(0 != (r = xcd_recorder_print(recorder, "Build fingerprint: '%s'\n",  props->build_fingerprint))) return r;
    if(0 != (r = xcd_recorder_print(recorder, "Revision: '%s'\n",           props->revision))) return r;
    if(0 != (r = xcd_recorder_print(recorder, "ABI: '%s'\n",                XCC_UTIL_ABI_STRING))) return r;

    return 0;
}
