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

#ifndef XCC_UTIL_H
#define XCC_UTIL_H 1

#include <stdint.h>
#include <sys/types.h>
#include <signal.h>

#ifdef __cplusplus
extern "C" {
#endif

#define XCC_UTIL_XCRASH_FILENAME        "libxcrash.so"
#define XCC_UTIL_XCRASH_DUMPER_FILENAME "libxcrash_dumper.so"

#define XCC_UTIL_CRASH_TYPE "native"

#if defined(__LP64__)
#define XCC_UTIL_FMT_ADDR "16"PRIxPTR
#else
#define XCC_UTIL_FMT_ADDR "8"PRIxPTR
#endif

#define XCC_UTIL_MAX(a,b) ({         \
            __typeof__ (a) _a = (a); \
            __typeof__ (b) _b = (b); \
            _a > _b ? _a : _b; })

#define XCC_UTIL_MIN(a,b) ({         \
            __typeof__ (a) _a = (a); \
            __typeof__ (b) _b = (b); \
            _a < _b ? _a : _b; })

#ifndef TEMP_FAILURE_RETRY
#define TEMP_FAILURE_RETRY(exp) ({         \
    __typeof__(exp) _rc;                   \
    do {                                   \
        errno = 0;                         \
        _rc = (exp);                       \
    } while (_rc == -1 && errno == EINTR); \
    _rc; })
#endif

#if defined(__arm__)
#define XCC_UTIL_ABI_STRING "arm"
#elif defined(__aarch64__)
#define XCC_UTIL_ABI_STRING "arm64"
#elif defined(__i386__)
#define XCC_UTIL_ABI_STRING "x86"
#elif defined(__x86_64__)
#define XCC_UTIL_ABI_STRING "x86_64"
#else
#define XCC_UTIL_ABI_STRING "unknown"
#endif

#define XCC_UTIL_TOMB_HEAD  "*** *** *** *** *** *** *** *** *** *** *** *** *** *** *** ***\n"
#define XCC_UTIL_THREAD_SEP "--- --- --- --- --- --- --- --- --- --- --- --- --- --- --- ---\n"
#define XCC_UTIL_THREAD_END "+++ +++ +++ +++ +++ +++ +++ +++ +++ +++ +++ +++ +++ +++ +++ +++\n"

typedef struct
{
    int   api_level;
    char *os_version;
    char *abi_list;
    char *manufacturer;
    char *brand;
    char *model;
    char *build_fingerprint;
    char *revision;
} xcc_util_build_prop_t;

void xcc_util_load_build_prop(xcc_util_build_prop_t *prop);

const char* xcc_util_get_signame(const siginfo_t* si);
const char* xcc_util_get_sigcodename(const siginfo_t* si);
int xcc_util_signal_has_si_addr(const siginfo_t* si);
int xcc_util_signal_has_sender(const siginfo_t* si, pid_t caller_pid);

char *xcc_util_trim(char *start);
int xcc_util_atoi(const char *str, int *i);

int xcc_util_write(int fd, const char *buf, size_t len);
int xcc_util_write_str(int fd, const char *str);
int xcc_util_write_format(int fd, const char *format, ...);

char *xcc_util_gets(char *s, int size, int fd);
int xcc_util_read_file_line(const char *path, char *buf, size_t len);

int xcc_util_get_process_name(pid_t pid, char *buf, size_t len);
int xcc_util_get_thread_name(pid_t tid, char *buf, size_t len);
int xcc_util_is_root();

#ifdef __cplusplus
}
#endif

#endif
