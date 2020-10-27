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

// Created by caikelun on 2019-08-20.

#include <stdint.h>
#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <jni.h>
#include "xcc_errno.h"
#include "xcc_fmt.h"
#include "xcc_util.h"
#include "xc_common.h"
#include "xc_jni.h"
#include "xc_util.h"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wgnu-statement-expression"
#pragma clang diagnostic ignored "-Wcast-align"

#define XC_COMMON_OPEN_DIR_FLAGS      (O_RDONLY | O_DIRECTORY | O_CLOEXEC)
#define XC_COMMON_OPEN_FILE_FLAGS     (O_RDWR | O_CLOEXEC)
#define XC_COMMON_OPEN_NEW_FILE_FLAGS (O_CREAT | O_WRONLY | O_CLOEXEC | O_TRUNC | O_APPEND)
#define XC_COMMON_OPEN_NEW_FILE_MODE  (S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH) //644

//system info
int           xc_common_api_level         = 0;
char         *xc_common_os_version        = NULL;
char         *xc_common_abi_list          = NULL;
char         *xc_common_manufacturer      = NULL;
char         *xc_common_brand             = NULL;
char         *xc_common_model             = NULL;
char         *xc_common_build_fingerprint = NULL;
char         *xc_common_kernel_version    = NULL;
long          xc_common_time_zone         = 0;

//app info
char         *xc_common_app_id            = NULL;
char         *xc_common_app_version       = NULL;
char         *xc_common_app_lib_dir       = NULL;
char         *xc_common_log_dir           = NULL;

//process info
pid_t         xc_common_process_id        = 0;
char         *xc_common_process_name      = NULL;
uint64_t      xc_common_start_time        = 0;
JavaVM       *xc_common_vm                = NULL;
jclass        xc_common_cb_class          = NULL;
int           xc_common_fd_null           = -1;

//process statue
sig_atomic_t  xc_common_native_crashed    = 0;
sig_atomic_t  xc_common_java_crashed      = 0;

static int    xc_common_crash_prepared_fd = -1;
static int    xc_common_trace_prepared_fd = -1;

static void xc_common_open_prepared_fd(int is_crash)
{
    int fd = (is_crash ? xc_common_crash_prepared_fd : xc_common_trace_prepared_fd);
    if(fd >= 0) return;
    
    fd = XCC_UTIL_TEMP_FAILURE_RETRY(open("/dev/null", O_RDWR));
    
    if(is_crash)
        xc_common_crash_prepared_fd = fd;
    else
        xc_common_trace_prepared_fd = fd;
}

static int xc_common_close_prepared_fd(int is_crash)
{
    int fd = (is_crash ? xc_common_crash_prepared_fd : xc_common_trace_prepared_fd);
    if(fd < 0) return XCC_ERRNO_FD;
    
    close(fd);

    if(is_crash)
        xc_common_crash_prepared_fd = -1;
    else
        xc_common_trace_prepared_fd = -1;

    return 0;
}

void xc_common_set_vm(JavaVM *vm, JNIEnv *env, jclass cls)
{
    xc_common_vm = vm;

    xc_common_cb_class = (*env)->NewGlobalRef(env, cls);
    XC_JNI_CHECK_NULL_AND_PENDING_EXCEPTION(xc_common_cb_class, err);
    return;

 err:
    xc_common_cb_class = NULL;
}

int xc_common_init(int         api_level,
                   const char *os_version,
                   const char *abi_list,
                   const char *manufacturer,
                   const char *brand,
                   const char *model,
                   const char *build_fingerprint,
                   const char *app_id,
                   const char *app_version,
                   const char *app_lib_dir,
                   const char *log_dir)
{
    int             r = 0;
    struct timeval  tv;
    struct tm       tm;
    char            buf[256];
    char           *kernel_version;
    char           *process_name;

#define XC_COMMON_DUP_STR(v) do {                                       \
        if(NULL == v || 0 == strlen(v))                                 \
            xc_common_##v = "unknown";                                  \
        else                                                            \
        {                                                               \
            if(NULL == (xc_common_##v = strdup(v)))                     \
            {                                                           \
                r = XCC_ERRNO_NOMEM;                                    \
                goto err;                                               \
            }                                                           \
        }                                                               \
    } while (0)

#define XC_COMMON_FREE_STR(v) do {                                      \
        if(NULL != xc_common_##v) {                                     \
            free(xc_common_##v);                                        \
            xc_common_##v = NULL;                                       \
        }                                                               \
    } while (0)

    //save start time
    if(0 != gettimeofday(&tv, NULL)) return XCC_ERRNO_SYS;
    xc_common_start_time = (uint64_t)(tv.tv_sec) * 1000 * 1000 + (uint64_t)tv.tv_usec;
 
    //save time zone
    if(NULL == localtime_r((time_t*)(&(tv.tv_sec)), &tm)) return XCC_ERRNO_SYS;
    xc_common_time_zone = tm.tm_gmtoff;

    //save common info
    xc_common_api_level = api_level;
    XC_COMMON_DUP_STR(os_version);
    XC_COMMON_DUP_STR(abi_list);
    XC_COMMON_DUP_STR(manufacturer);
    XC_COMMON_DUP_STR(brand);
    XC_COMMON_DUP_STR(model);
    XC_COMMON_DUP_STR(build_fingerprint);
    XC_COMMON_DUP_STR(app_id);
    XC_COMMON_DUP_STR(app_version);
    XC_COMMON_DUP_STR(app_lib_dir);
    XC_COMMON_DUP_STR(log_dir);
    
    //save kernel version
    xc_util_get_kernel_version(buf, sizeof(buf));
    kernel_version = buf;
    XC_COMMON_DUP_STR(kernel_version);

    //save process id and process name
    xc_common_process_id = getpid();
    xcc_util_get_process_name(xc_common_process_id, buf, sizeof(buf));
    process_name = buf;
    XC_COMMON_DUP_STR(process_name);

    //to /dev/null
    if((xc_common_fd_null = XCC_UTIL_TEMP_FAILURE_RETRY(open("/dev/null", O_RDWR))) < 0)
    {
        r = XCC_ERRNO_SYS;
        goto err;
    }

    //check or create log directory
    if(0 != (r = xc_util_mkdirs(log_dir))) goto err;

    //create prepared FD for FD exhausted case
    xc_common_open_prepared_fd(1);
    xc_common_open_prepared_fd(0);

    return 0;

 err:
    XC_COMMON_FREE_STR(os_version);
    XC_COMMON_FREE_STR(abi_list);
    XC_COMMON_FREE_STR(manufacturer);
    XC_COMMON_FREE_STR(brand);
    XC_COMMON_FREE_STR(model);
    XC_COMMON_FREE_STR(build_fingerprint);
    XC_COMMON_FREE_STR(app_id);
    XC_COMMON_FREE_STR(app_version);
    XC_COMMON_FREE_STR(app_lib_dir);
    XC_COMMON_FREE_STR(log_dir);
    XC_COMMON_FREE_STR(kernel_version);
    XC_COMMON_FREE_STR(process_name);
    
    return r;
}

static int xc_common_open_log(int is_crash, uint64_t timestamp,
                              char *pathname, size_t pathname_len, int *from_placeholder)
{
    int                fd = -1;
    char               buf[512];
    char               placeholder_pathname[1024];
    long               n, i;
    xcc_util_dirent_t *ent;

    xcc_fmt_snprintf(pathname, pathname_len, "%s/"XC_COMMON_LOG_PREFIX"_%020"PRIu64"_%s__%s%s",
                     xc_common_log_dir, timestamp, xc_common_app_version, xc_common_process_name,
                     is_crash ? XC_COMMON_LOG_SUFFIX_CRASH : XC_COMMON_LOG_SUFFIX_TRACE);

    //open dir
    if((fd = XCC_UTIL_TEMP_FAILURE_RETRY(open(xc_common_log_dir, XC_COMMON_OPEN_DIR_FLAGS))) < 0)
    {
        //try again with the prepared fd
        if(0 != xc_common_close_prepared_fd(is_crash)) goto create_new_file;
        if((fd = XCC_UTIL_TEMP_FAILURE_RETRY(open(xc_common_log_dir, XC_COMMON_OPEN_DIR_FLAGS))) < 0) goto create_new_file;
    }

    //try to rename a placeholder file and open it
    while((n = syscall(XCC_UTIL_SYSCALL_GETDENTS, fd, buf, sizeof(buf))) > 0)
    {
        for(i = 0; i < n; i += ent->d_reclen)
        {
            ent = (xcc_util_dirent_t *)(buf + i);
            
            // placeholder_01234567890123456789.clean.xcrash
            // file name length: 45
            if(45 == strlen(ent->d_name) &&
               0 == memcmp(ent->d_name, XC_COMMON_PLACEHOLDER_PREFIX"_", 12) &&
               0 == memcmp(ent->d_name + 32, XC_COMMON_PLACEHOLDER_SUFFIX, 13))
            {
                xcc_fmt_snprintf(placeholder_pathname, sizeof(placeholder_pathname), "%s/%s", xc_common_log_dir, ent->d_name);
                if(0 == rename(placeholder_pathname, pathname))
                {
                    close(fd);
                    if(NULL != from_placeholder) *from_placeholder = 1;
                    return XCC_UTIL_TEMP_FAILURE_RETRY(open(pathname, XC_COMMON_OPEN_FILE_FLAGS));
                }
            }
        }
    }
    close(fd);
    xc_common_open_prepared_fd(is_crash);
    
 create_new_file:
    if(NULL != from_placeholder) *from_placeholder = 0;
    
    if((fd = XCC_UTIL_TEMP_FAILURE_RETRY(open(pathname, XC_COMMON_OPEN_NEW_FILE_FLAGS, XC_COMMON_OPEN_NEW_FILE_MODE))) >= 0) return fd;

    //try again with the prepared fd
    if(0 != xc_common_close_prepared_fd(is_crash)) return -1;
    return XCC_UTIL_TEMP_FAILURE_RETRY(open(pathname, XC_COMMON_OPEN_NEW_FILE_FLAGS, XC_COMMON_OPEN_NEW_FILE_MODE));
}

static void xc_common_close_log(int fd, int is_crash)
{
    close(fd);
    xc_common_open_prepared_fd(is_crash);
}

int xc_common_open_crash_log(char *pathname, size_t pathname_len, int *from_placeholder)
{
    return xc_common_open_log(1, xc_common_start_time, pathname, pathname_len, from_placeholder);
}

int xc_common_open_trace_log(char *pathname, size_t pathname_len, uint64_t trace_time)
{
    return xc_common_open_log(0, trace_time, pathname, pathname_len, NULL);
}

void xc_common_close_crash_log(int fd)
{
    xc_common_close_log(fd, 1);
}

void xc_common_close_trace_log(int fd)
{
    xc_common_close_log(fd, 0);
}

int xc_common_seek_to_content_end(int fd)
{
    uint8_t buf[1024];
    ssize_t readed, n;
    off_t   offset = 0;

    //placeholder file
    if(lseek(fd, 0, SEEK_SET) < 0) goto err;
    while(1)
    {
        readed = XCC_UTIL_TEMP_FAILURE_RETRY(read(fd, buf, sizeof(buf)));
        if(readed < 0)
        {
            goto err;
        }
        else if(0 == readed)
        {
            if(lseek(fd, 0, SEEK_END) < 0) goto err;
            return fd;
        }
        else
        {
            for(n = readed; n > 0; n--)
            {
                if(0 != buf[n - 1]) break;
            }
            offset += (off_t)n;
            if(n < readed)
            {
                if(lseek(fd, offset, SEEK_SET) < 0) goto err;
                return fd;
            }
        }
    }

 err:
    close(fd);
    return -1;
}

#pragma clang diagnostic pop
