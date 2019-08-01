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

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wreserved-id-macro"
#define _GNU_SOURCE
#pragma clang diagnostic pop

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <inttypes.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include <stdio.h>
#include <pthread.h>
#include <sched.h>
#include <sys/prctl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/syscall.h>
#include <sys/uio.h>
#include <android/log.h>
#include "xcc_errno.h"
#include "xcc_spot.h"
#include "xcc_util.h"
#include "xcc_unwind.h"
#include "xcc_signal.h"
#include "xcc_b64.h"
#include "xc_core.h"
#include "xc_util.h"
#include "xc_jni.h"
#include "xc_recorder.h"
#include "xc_fallback.h"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wgnu-statement-expression"

#define XC_CORE_EMERGENCY_BUF_LEN (20 * 1024)
#define XC_CORE_ERR_TITLE         "\n\nxcrash error:\n"

static pthread_mutex_t        xc_core_mutex   = PTHREAD_MUTEX_INITIALIZER;
static int                    xc_core_handled = 0;
static int                    xc_core_inited  = 0;
static int                    xc_core_log_fd  = -1;

//global cache which used in signal handler
static char                  *xc_core_dumper_pathname;
static int                    xc_core_restore_signal_handler;
static char                  *xc_core_emergency;
static xcc_util_build_prop_t  xc_core_build_prop;
static xc_recorder_t         *xc_core_recorder;
static long                   xc_core_timezone;
static char                  *xc_core_kernel_version;

//for clone and fork
#ifndef __i386__
#define XC_CORE_CHILD_STACK_LEN   (16 * 1024)
static void                  *xc_core_child_stack;
#else
static int                    xc_core_child_notifier[2];
#endif

//info passed to the dumper process
static xcc_spot_t             xc_core_spot;
static char                  *xc_core_log_pathname = NULL;
static char                  *xc_core_app_id = "unknown";
static char                  *xc_core_app_version = "unknown";
static char                  *xc_core_dump_all_threads_whitelist = NULL;

static int xc_core_fork(int (*fn)(void *))
{
#ifndef __i386__
    return clone(fn, xc_core_child_stack, CLONE_VFORK | CLONE_FS | CLONE_UNTRACED, NULL);
#else
    pid_t dumper_pid = fork();
    if(-1 == dumper_pid)
    {
        return -1;
    }
    else if(0 == dumper_pid)
    {
        //child process ...
        char msg = 'a';
        XCC_UTIL_TEMP_FAILURE_RETRY(write(xc_core_child_notifier[1], &msg, sizeof(char)));
        syscall(SYS_close, xc_core_child_notifier[0]);
        syscall(SYS_close, xc_core_child_notifier[1]);

        _exit(fn(NULL));
    }
    else
    {
        //parent process ...
        char msg;
        XCC_UTIL_TEMP_FAILURE_RETRY(read(xc_core_child_notifier[0], &msg, sizeof(char)));
        syscall(SYS_close, xc_core_child_notifier[0]);
        syscall(SYS_close, xc_core_child_notifier[1]);

        return dumper_pid;
    }
#endif
}

static int xc_core_exec_dumper(void *arg)
{
    (void)arg;

    //for fd exhaust
    //keep the log_fd open for writing error msg before execl()
    int i;
    for(i = 0; i < 1024; i++)
        if(i != xc_core_log_fd)
            syscall(SYS_close, i);

    //hold the fd 0, 1, 2
    errno = 0;
    int devnull = XCC_UTIL_TEMP_FAILURE_RETRY(open("/dev/null", O_RDWR));
    if(devnull < 0)
    {
        xcc_util_write_format_safe(xc_core_log_fd, XC_CORE_ERR_TITLE"open /dev/null failed, errno=%d\n\n", errno);
        return 90;
    }
    else if(0 != devnull)
    {
        xcc_util_write_format_safe(xc_core_log_fd, XC_CORE_ERR_TITLE"/dev/null fd NOT 0, errno=%d\n\n", errno);
        return 91;
    }
    XCC_UTIL_TEMP_FAILURE_RETRY(dup2(devnull, STDOUT_FILENO));
    XCC_UTIL_TEMP_FAILURE_RETRY(dup2(devnull, STDERR_FILENO));
    
    //create args pipe
    int pipefd[2];
    errno = 0;
    if(0 != pipe2(pipefd, O_CLOEXEC))
    {
        xcc_util_write_format_safe(xc_core_log_fd, XC_CORE_ERR_TITLE"create args pipe failed, errno=%d\n\n", errno);
        return 92;
    }

    //set args pipe size
    //range: pagesize (4K) ~ /proc/sys/fs/pipe-max-size (1024K)
    int write_len = (int)(sizeof(xcc_spot_t) + xc_core_spot.log_pathname_len + xc_core_spot.app_id_len
                          + xc_core_spot.app_version_len + xc_core_spot.dump_all_threads_whitelist_len);
    errno = 0;
    if(fcntl(pipefd[1], F_SETPIPE_SZ, write_len) < write_len)
    {
        xcc_util_write_format_safe(xc_core_log_fd, XC_CORE_ERR_TITLE"set args pipe size failed, errno=%d\n\n", errno);
        return 93;
    }

    //write args to pipe
    struct iovec iovs[5] = {
        {.iov_base = &xc_core_spot, .iov_len = sizeof(xcc_spot_t)},
        {.iov_base = xc_core_log_pathname, .iov_len = xc_core_spot.log_pathname_len},
        {.iov_base = xc_core_app_id, .iov_len = xc_core_spot.app_id_len},
        {.iov_base = xc_core_app_version, .iov_len = xc_core_spot.app_version_len},
        {.iov_base = xc_core_dump_all_threads_whitelist, .iov_len = xc_core_spot.dump_all_threads_whitelist_len}
    };
    int iovs_cnt = (0 == xc_core_spot.dump_all_threads_whitelist_len ? 4 : 5);
    errno = 0;
    ssize_t ret = XCC_UTIL_TEMP_FAILURE_RETRY(writev(pipefd[1], iovs, iovs_cnt));
    if((ssize_t)write_len != ret)
    {
        xcc_util_write_format_safe(xc_core_log_fd, XC_CORE_ERR_TITLE"write args to pipe failed, return=%d, errno=%d\n\n", ret, errno);
        return 94;
    }

    //copy the read-side of the args-pipe to stdin (fd: 0)
    XCC_UTIL_TEMP_FAILURE_RETRY(dup2(pipefd[0], STDIN_FILENO));
    
    syscall(SYS_close, pipefd[0]);
    syscall(SYS_close, pipefd[1]);

    //escape to the dumper process
    errno = 0;
    execl(xc_core_dumper_pathname, XCC_UTIL_XCRASH_DUMPER_FILENAME, NULL);
    return 100 + errno;
}

static void xc_core_signal_handler(int sig, siginfo_t *si, void *uc)
{
    struct timespec crash_tp;
    int             restore_orig_ptracer = 0;
    int             restore_orig_dumpable = 0;
    int             orig_dumpable = 0;
    int             dump_ok = 0;

    (void)sig;

    pthread_mutex_lock(&xc_core_mutex);

    //only once
    if(xc_core_handled) goto exit;
    xc_core_handled = 1;

    //restore the original/default signal handler
    if(xc_core_restore_signal_handler)
    {
        if(0 != xcc_signal_unregister()) goto exit;
    }
    else
    {
        if(0 != xcc_signal_ignore()) goto exit;
    }

    //save crash spot info
    clock_gettime(CLOCK_REALTIME, &crash_tp);
    xc_core_spot.crash_time = (uint64_t)(crash_tp.tv_sec) * 1000 * 1000 + (uint64_t)crash_tp.tv_nsec / 1000;
    xc_core_spot.crash_pid = getpid();
    xc_core_spot.crash_tid = gettid();
    memcpy(&(xc_core_spot.siginfo), si, sizeof(siginfo_t));
    memcpy(&(xc_core_spot.ucontext), uc, sizeof(ucontext_t));

    //create log file
    if((xc_core_log_fd = xc_recorder_create_and_open(xc_core_recorder)) < 0) goto end;

    //check privilege-restricting mode
    //https://www.kernel.org/doc/Documentation/prctl/no_new_privs.txt
    //errno = 0;
    //if(1 == prctl(PR_GET_NO_NEW_PRIVS, 0, 0, 0, 0))
    //{
    //    xcc_util_write_format_safe(xc_core_log_fd, XC_CORE_ERR_TITLE"get NO_NEW_PRIVS failed, errno=%d\n\n", errno);
    //    goto end;
    //}

    //set dumpable
    orig_dumpable = prctl(PR_GET_DUMPABLE);
    errno = 0;
    if(0 != prctl(PR_SET_DUMPABLE, 1))
    {
        xcc_util_write_format_safe(xc_core_log_fd, XC_CORE_ERR_TITLE"set dumpable failed, errno=%d\n\n", errno);
        goto end;
    }
    restore_orig_dumpable = 1;

    //set traceable (disable the ptrace restrictions introduced by Yama)
    //https://www.kernel.org/doc/Documentation/security/Yama.txt
    errno = 0;
    if(0 != prctl(PR_SET_PTRACER, PR_SET_PTRACER_ANY))
    {
        if(EINVAL == errno)
        {
            //this kernel does not support PR_SET_PTRACER_ANY, or Yama is not enabled
            ;
        }
        else
        {
            xcc_util_write_format_safe(xc_core_log_fd, XC_CORE_ERR_TITLE"set traceable failed, errno=%d\n\n", errno);
            goto end;
        }
    }
    else
    {
        restore_orig_ptracer = 1;
    }

    //spawn crash dumper process
    errno = 0;
    pid_t dumper_pid = xc_core_fork(xc_core_exec_dumper);
    if(-1 == dumper_pid)
    {
        xcc_util_write_format_safe(xc_core_log_fd, XC_CORE_ERR_TITLE"fork failed, errno=%d\n\n", errno);
        goto end;
    }

    //parent process ...

    //wait the crash dumper process terminated
    errno = 0;
    int status = 0;
    int r = XCC_UTIL_TEMP_FAILURE_RETRY(waitpid(dumper_pid, &status, __WALL));

    //the crash dumper process should have written a lot of logs,
    //so we need to seek to the end of log file
    if((xc_core_log_fd = xc_recorder_seek_to_end(xc_core_recorder, xc_core_log_fd)) < 0) goto end;
    
    if(-1 == r)
    {
        xcc_util_write_format_safe(xc_core_log_fd, XC_CORE_ERR_TITLE"waitpid failed, errno=%d\n\n", errno);
        goto end;
    }

    //check child process state
    if(!(WIFEXITED(status)) || 0 != WEXITSTATUS(status))
    {
        if(WIFEXITED(status) && 0 != WEXITSTATUS(status))
        {
            //terminated normally, but return / exit / _exit NON-zero
            xcc_util_write_format_safe(xc_core_log_fd, XC_CORE_ERR_TITLE"child terminated normally with non-zero exit status(%d), dumper=%s\n\n", WEXITSTATUS(status), xc_core_dumper_pathname);
            goto end;
        }
        else if(WIFSIGNALED(status))
        {
            //terminated by a signal
            xcc_util_write_format_safe(xc_core_log_fd, XC_CORE_ERR_TITLE"child terminated by a signal(%d)\n\n", WTERMSIG(status));
            goto end;
        }
        else
        {
            xcc_util_write_format_safe(xc_core_log_fd, XC_CORE_ERR_TITLE"child terminated with other error status(%d), dumper=%s\n\n", status, xc_core_dumper_pathname);
            goto end;
        }
    }

    //check the backtrace
    if(!xc_recorder_check_backtrace_valid(xc_core_recorder)) goto end;
    
    dump_ok = 1;

 end:
    //restore dumpable
    if(restore_orig_dumpable) prctl(PR_SET_DUMPABLE, orig_dumpable);

    //restore traceable
    if(restore_orig_ptracer) prctl(PR_SET_PTRACER, 0);

    //fallback
    if(!dump_ok)
    {
        xc_fallback_get_emergency(si,
                                  (ucontext_t *)uc,
                                  xc_core_spot.crash_pid,
                                  xc_core_spot.crash_tid,
                                  xc_core_timezone,
                                  xc_core_spot.start_time,
                                  xc_core_spot.crash_time,
                                  xc_core_app_id,
                                  xc_core_app_version,
                                  xc_core_build_prop.api_level,
                                  xc_core_build_prop.os_version,
                                  xc_core_kernel_version,
                                  xc_core_build_prop.abi_list,
                                  xc_core_build_prop.manufacturer,
                                  xc_core_build_prop.brand,
                                  xc_core_build_prop.model,
                                  xc_core_build_prop.build_fingerprint,
                                  xc_core_build_prop.revision,
                                  xc_core_emergency,
                                  XC_CORE_EMERGENCY_BUF_LEN);

        if(xc_core_log_fd >= 0)
        {
            if(0 != xc_fallback_record(xc_core_log_fd,
                                       xc_core_emergency,
                                       xc_core_spot.crash_pid,
                                       xc_core_build_prop.api_level,
                                       xc_core_spot.logcat_system_lines,
                                       xc_core_spot.logcat_events_lines,
                                       xc_core_spot.logcat_main_lines))
            {
                close(xc_core_log_fd);
                xc_core_log_fd = -1;
            }
        }
    }

    //we have written all the required information in the native layer, close the FD
    if(xc_core_log_fd >= 0) close(xc_core_log_fd);

    //jni callback
    xc_jni_callback(xc_core_log_pathname, '\0' == xc_core_emergency[0] ? NULL : xc_core_emergency);

    if(0 != xcc_signal_resend(si)) goto exit;
    
    pthread_mutex_unlock(&xc_core_mutex);
    return;

 exit:
    pthread_mutex_unlock(&xc_core_mutex);
    _exit(1);
}

static void xc_core_set_dump_all_threads_whitelist(const char **whitelist, size_t whitelist_len)
{
    size_t  i, len;
    size_t  encoded_len, total_encoded_len = 0, cur_encoded_len = 0;
    char   *total_encoded_whitelist, *tmp;
    
    if(NULL == whitelist || 0 == whitelist_len) return;

    //get total encoded length
    for(i = 0; i < whitelist_len; i++)
    {
        if(NULL == whitelist[i]) continue;
        len = strlen(whitelist[i]);
        if(0 == len) continue;
        total_encoded_len += xcc_b64_encode_max_len(len);
    }
    if(0 == total_encoded_len) return;
    total_encoded_len += whitelist_len; //separator ('|')
    total_encoded_len += 1; //terminating null byte ('\0')

    //alloc encode buffer
    if(NULL == (total_encoded_whitelist = calloc(1, total_encoded_len))) return;

    //to base64 encode each whitelist item
    for(i = 0; i < whitelist_len; i++)
    {
        if(NULL == whitelist[i]) continue;
        len = strlen(whitelist[i]);
        if(0 == len) continue;

        if(NULL != (tmp = xcc_b64_encode((const uint8_t *)(whitelist[i]), len, &encoded_len)))
        {
            if(cur_encoded_len + encoded_len + 1 >= total_encoded_len) return; //impossible
            
            memcpy(total_encoded_whitelist + cur_encoded_len, tmp, encoded_len);
            cur_encoded_len += encoded_len;
            
            memcpy(total_encoded_whitelist + cur_encoded_len, "|", 1);
            cur_encoded_len += 1;
            
            free(tmp);
        }
    }

    if(cur_encoded_len > 0 && '|' == total_encoded_whitelist[cur_encoded_len - 1])
    {
        total_encoded_whitelist[cur_encoded_len - 1] = '\0';
        cur_encoded_len -= 1;
    }

    if(0 == cur_encoded_len)
    {
        free(total_encoded_whitelist);
        return;
    }

    xc_core_spot.dump_all_threads_whitelist_len = cur_encoded_len;
    xc_core_dump_all_threads_whitelist = total_encoded_whitelist;
}

int xc_core_init(int restore_signal_handler,
                 const char *app_id,
                 const char *app_version,
                 const char *app_lib_dir,
                 const char *log_dir,
                 unsigned int logcat_system_lines,
                 unsigned int logcat_events_lines,
                 unsigned int logcat_main_lines,
                 int dump_elf_hash,
                 int dump_map,
                 int dump_fds,
                 int dump_all_threads,
                 int dump_all_threads_count_max,
                 const char **dump_all_threads_whitelist,
                 size_t dump_all_threads_whitelist_len)
{
    struct timeval  tv;
    struct tm       tm;
    uint64_t        start_time;
    int             r;
    char            buf[256];

#if 0
    __android_log_print(ANDROID_LOG_DEBUG, "xcrash",
                        "CORE: init: restore_signal_handler=%d, "
                        "app_id=%s, "
                        "app_version=%s, "
                        "app_lib_dir=%s, "
                        "log_dir=%s, "
                        "logcat_system_lines=%u, "
                        "logcat_events_lines=%u, "
                        "logcat_main_lines=%u, "
                        "dump_elf_hash=%d, "
                        "dump_map=%d, "
                        "dump_fds=%d, "
                        "dump_all_threads=%d, "
                        "dump_all_threads_count_max=%d, "
                        "dump_all_threads_whitelist_len=%zu",
                        restore_signal_handler,
                        app_id,
                        app_version,
                        app_lib_dir,
                        log_dir,
                        logcat_system_lines,
                        logcat_events_lines,
                        logcat_main_lines,
                        dump_elf_hash,
                        dump_map,
                        dump_fds,
                        dump_all_threads,
                        dump_all_threads_count_max,
                        dump_all_threads_whitelist_len);

    if(NULL != dump_all_threads_whitelist && dump_all_threads_whitelist_len > 0)
    {
        size_t i;
        for(i = 0; i < dump_all_threads_whitelist_len; i++)
        {
            if(NULL != dump_all_threads_whitelist[i] && strlen(dump_all_threads_whitelist[i]) > 0)
                __android_log_print(ANDROID_LOG_DEBUG, "xcrash",
                                    "CORE: init: whitelist_item: %s",
                                    dump_all_threads_whitelist[i]);
        }
    }

#endif

    if(NULL == app_lib_dir || NULL == log_dir) return XCC_ERRNO_INVAL;

    //init only once
    if(xc_core_inited) return 0;
    xc_core_inited = 1;

    //get start time
    if(0 != gettimeofday(&tv, NULL)) return XCC_ERRNO_SYS;
    start_time = (uint64_t)(tv.tv_sec) * 1000 * 1000 + (uint64_t)tv.tv_usec;
 
    //get time zone
    if(NULL == localtime_r((time_t*)(&(tv.tv_sec)), &tm)) return XCC_ERRNO_SYS;
    xc_core_timezone = tm.tm_gmtoff;

    //get system property
    xcc_util_load_build_prop(&xc_core_build_prop);

    //get kernel version
    xcc_util_get_kernel_version(buf, sizeof(buf));
    if(NULL == (xc_core_kernel_version = strdup(buf))) return XCC_ERRNO_NOMEM;

    //init the tombstone file recorder
    if(0 != (r = xcd_recorder_create(&xc_core_recorder, start_time, app_version, log_dir, &xc_core_log_pathname))) return r;

    //strings passed to the dumper process
    if(NULL != app_id)
        if(NULL == (xc_core_app_id = strdup(app_id))) return XCC_ERRNO_NOMEM;
    if(NULL != app_version)
        if(NULL == (xc_core_app_version = strdup(app_version))) return XCC_ERRNO_NOMEM;

    //struct info passed to the dumper process
    memset(&xc_core_spot, 0, sizeof(xcc_spot_t));
    xc_core_spot.start_time = start_time;
    xc_core_spot.logcat_system_lines = logcat_system_lines;
    xc_core_spot.logcat_events_lines = logcat_events_lines;
    xc_core_spot.logcat_main_lines = logcat_main_lines;
    xc_core_spot.dump_elf_hash = dump_elf_hash;
    xc_core_spot.dump_map = dump_map;
    xc_core_spot.dump_fds = dump_fds;
    xc_core_spot.dump_all_threads = dump_all_threads;
    xc_core_spot.dump_all_threads_count_max = dump_all_threads_count_max;
    xc_core_spot.log_pathname_len = strlen(xc_core_log_pathname);
    xc_core_spot.app_id_len = strlen(xc_core_app_id);
    xc_core_spot.app_version_len = strlen(xc_core_app_version);

    //info about threads whitelist
    xc_core_set_dump_all_threads_whitelist(dump_all_threads_whitelist, dump_all_threads_whitelist_len);
#if 0
    __android_log_print(ANDROID_LOG_DEBUG, "xcrash",
                        "CORE: init: encoded_whitelist_len=%zu, encoded_whitelist=%s",
                        xc_core_spot.dump_all_threads_whitelist_len,
                        (NULL != xc_core_dump_all_threads_whitelist ? xc_core_dump_all_threads_whitelist : "(NULL)"));
#endif

    //others
    xc_core_restore_signal_handler = restore_signal_handler;
    if(NULL == (xc_core_emergency = calloc(XC_CORE_EMERGENCY_BUF_LEN, 1))) return XCC_ERRNO_NOMEM;
    if(NULL == (xc_core_dumper_pathname = xc_util_strdupcat(app_lib_dir, "/"XCC_UTIL_XCRASH_DUMPER_FILENAME))) return XCC_ERRNO_NOMEM;

    //for clone and fork
#ifndef __i386__
    if(NULL == (xc_core_child_stack = calloc(XC_CORE_CHILD_STACK_LEN, 1))) return XCC_ERRNO_NOMEM;
    xc_core_child_stack = (void *)(((uint8_t *)xc_core_child_stack) + XC_CORE_CHILD_STACK_LEN);
#else
    if(0 != pipe2(xc_core_child_notifier, O_CLOEXEC)) return XCC_ERRNO_SYS;
#endif

    //register signal handler
    if(0 != (r = xcc_signal_register(xc_core_signal_handler))) return r;
    
    return 0;
}

#pragma clang diagnostic pop
