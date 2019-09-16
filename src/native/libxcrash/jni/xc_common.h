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

#ifndef XC_COMMON_H
#define XC_COMMON_H 1

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <sys/types.h>
#include <jni.h>

// log filename format:
// tombstone_01234567890123456789_appversion__processname.native.xcrash
// tombstone_01234567890123456789_appversion__processname.trace.xcrash
// placeholder_01234567890123456789.clean.xcrash
#define XC_COMMON_LOG_PREFIX           "tombstone"
#define XC_COMMON_LOG_PREFIX_LEN       9
#define XC_COMMON_LOG_SUFFIX_CRASH     ".native.xcrash"
#define XC_COMMON_LOG_SUFFIX_TRACE     ".trace.xcrash"
#define XC_COMMON_LOG_SUFFIX_TRACE_LEN 13
#define XC_COMMON_LOG_NAME_MIN_TRACE   (9 + 1 + 20 + 1 + 2 + 13)
#define XC_COMMON_PLACEHOLDER_PREFIX   "placeholder"
#define XC_COMMON_PLACEHOLDER_SUFFIX   ".clean.xcrash"

//system info
extern int           xc_common_api_level;
extern char         *xc_common_os_version;
extern char         *xc_common_abi_list;
extern char         *xc_common_manufacturer;
extern char         *xc_common_brand;
extern char         *xc_common_model;
extern char         *xc_common_build_fingerprint;
extern char         *xc_common_kernel_version;
extern long          xc_common_time_zone;

//app info
extern char         *xc_common_app_id;
extern char         *xc_common_app_version;
extern char         *xc_common_app_lib_dir;
extern char         *xc_common_log_dir;

//process info
extern pid_t         xc_common_process_id;
extern char         *xc_common_process_name;
extern uint64_t      xc_common_start_time;
extern JavaVM       *xc_common_vm;
extern jclass        xc_common_cb_class;
extern int           xc_common_fd_null;

//process statue
extern sig_atomic_t  xc_common_native_crashed;
extern sig_atomic_t  xc_common_java_crashed;

void xc_common_set_vm(JavaVM *vm, JNIEnv *env, jclass cls);

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
                   const char *log_dir);

int xc_common_open_crash_log(char *pathname, size_t pathname_len, int *from_placeholder);
int xc_common_open_trace_log(char *pathname, size_t pathname_len, uint64_t trace_time);
void xc_common_close_crash_log(int fd);
void xc_common_close_trace_log(int fd);
int xc_common_seek_to_content_end(int fd);

#ifdef __cplusplus
}
#endif

#endif
