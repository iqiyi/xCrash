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
#include <sys/eventfd.h>
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
#include "xcc_util.h"
#include "xc_crash.h"
#include "xc_common.h"
#include "xc_dl.h"
#include "xc_util.h"
#include "xc_jni.h"
#include "xc_fallback.h"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wgnu-statement-expression"

#define XC_CRASH_CALLBACK_METHOD_NAME      "crashCallback"
#define XC_CRASH_CALLBACK_METHOD_SIGNATURE "(Ljava/lang/String;Ljava/lang/String;ZZLjava/lang/String;)V"
#define XC_CRASH_EMERGENCY_BUF_LEN         (30 * 1024)
#define XC_CRASH_ERR_TITLE                 "\n\nxcrash error:\n"

static pthread_mutex_t  xc_crash_mutex   = PTHREAD_MUTEX_INITIALIZER;
static int              xc_crash_rethrow;
static char            *xc_crash_dumper_pathname;
static char            *xc_crash_emergency;

//the log file
static int              xc_crash_prepared_fd = -1;
static int              xc_crash_log_fd  = -1;
static int              xc_crash_log_from_placeholder;
static char             xc_crash_log_pathname[1024] = "\0";

//the crash
static pid_t            xc_crash_tid = 0;
static int              xc_crash_dump_java_stacktrace = 0; //try to dump java stacktrace in java layer
static uint64_t         xc_crash_time = 0;

//callback
static jmethodID        xc_crash_cb_method = NULL;
static pthread_t        xc_crash_cb_thd;
static int              xc_crash_cb_notifier = -1;

//for clone and fork
#ifndef __i386__
#define XC_CRASH_CHILD_STACK_LEN (16 * 1024)
static void            *xc_crash_child_stack;
#else
static int              xc_crash_child_notifier[2];
#endif

//info passed to the dumper process
static xcc_spot_t       xc_crash_spot;
static char            *xc_crash_dump_all_threads_whitelist = NULL;

static int xc_crash_fork(int (*fn)(void *))
{
#ifndef __i386__
    return clone(fn, xc_crash_child_stack, CLONE_VFORK | CLONE_FS | CLONE_UNTRACED, NULL);
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
        XCC_UTIL_TEMP_FAILURE_RETRY(write(xc_crash_child_notifier[1], &msg, sizeof(char)));
        syscall(SYS_close, xc_crash_child_notifier[0]);
        syscall(SYS_close, xc_crash_child_notifier[1]);

        _exit(fn(NULL));
    }
    else
    {
        //parent process ...
        char msg;
        XCC_UTIL_TEMP_FAILURE_RETRY(read(xc_crash_child_notifier[0], &msg, sizeof(char)));
        syscall(SYS_close, xc_crash_child_notifier[0]);
        syscall(SYS_close, xc_crash_child_notifier[1]);

        return dumper_pid;
    }
#endif
}

static int xc_crash_exec_dumper(void *arg)
{
    (void)arg;

    //for fd exhaust
    //keep the log_fd open for writing error msg before execl()
    int i;
    for(i = 0; i < 1024; i++)
        if(i != xc_crash_log_fd)
            syscall(SYS_close, i);

    //hold the fd 0, 1, 2
    errno = 0;
    int devnull = XCC_UTIL_TEMP_FAILURE_RETRY(open("/dev/null", O_RDWR));
    if(devnull < 0)
    {
        xcc_util_write_format_safe(xc_crash_log_fd, XC_CRASH_ERR_TITLE"open /dev/null failed, errno=%d\n\n", errno);
        return 90;
    }
    else if(0 != devnull)
    {
        xcc_util_write_format_safe(xc_crash_log_fd, XC_CRASH_ERR_TITLE"/dev/null fd NOT 0, errno=%d\n\n", errno);
        return 91;
    }
    XCC_UTIL_TEMP_FAILURE_RETRY(dup2(devnull, STDOUT_FILENO));
    XCC_UTIL_TEMP_FAILURE_RETRY(dup2(devnull, STDERR_FILENO));
    
    //create args pipe
    int pipefd[2];
    errno = 0;
    if(0 != pipe2(pipefd, O_CLOEXEC))
    {
        xcc_util_write_format_safe(xc_crash_log_fd, XC_CRASH_ERR_TITLE"create args pipe failed, errno=%d\n\n", errno);
        return 92;
    }

    //set args pipe size
    //range: pagesize (4K) ~ /proc/sys/fs/pipe-max-size (1024K)
    int write_len = (int)(sizeof(xcc_spot_t) +
                          xc_crash_spot.log_pathname_len +
                          xc_crash_spot.os_version_len +
                          xc_crash_spot.kernel_version_len +
                          xc_crash_spot.abi_list_len +
                          xc_crash_spot.manufacturer_len +
                          xc_crash_spot.brand_len +
                          xc_crash_spot.model_len +
                          xc_crash_spot.build_fingerprint_len +
                          xc_crash_spot.app_id_len +
                          xc_crash_spot.app_version_len +
                          xc_crash_spot.dump_all_threads_whitelist_len);
    errno = 0;
    if(fcntl(pipefd[1], F_SETPIPE_SZ, write_len) < write_len)
    {
        xcc_util_write_format_safe(xc_crash_log_fd, XC_CRASH_ERR_TITLE"set args pipe size failed, errno=%d\n\n", errno);
        return 93;
    }

    //write args to pipe
    struct iovec iovs[12] = {
        {.iov_base = &xc_crash_spot,                      .iov_len = sizeof(xcc_spot_t)},
        {.iov_base = xc_crash_log_pathname,               .iov_len = xc_crash_spot.log_pathname_len},
        {.iov_base = xc_common_os_version,                .iov_len = xc_crash_spot.os_version_len},
        {.iov_base = xc_common_kernel_version,            .iov_len = xc_crash_spot.kernel_version_len},
        {.iov_base = xc_common_abi_list,                  .iov_len = xc_crash_spot.abi_list_len},
        {.iov_base = xc_common_manufacturer,              .iov_len = xc_crash_spot.manufacturer_len},
        {.iov_base = xc_common_brand,                     .iov_len = xc_crash_spot.brand_len},
        {.iov_base = xc_common_model,                     .iov_len = xc_crash_spot.model_len},
        {.iov_base = xc_common_build_fingerprint,         .iov_len = xc_crash_spot.build_fingerprint_len},
        {.iov_base = xc_common_app_id,                    .iov_len = xc_crash_spot.app_id_len},
        {.iov_base = xc_common_app_version,               .iov_len = xc_crash_spot.app_version_len},
        {.iov_base = xc_crash_dump_all_threads_whitelist, .iov_len = xc_crash_spot.dump_all_threads_whitelist_len}
    };
    int iovs_cnt = (0 == xc_crash_spot.dump_all_threads_whitelist_len ? 11 : 12);
    errno = 0;
    ssize_t ret = XCC_UTIL_TEMP_FAILURE_RETRY(writev(pipefd[1], iovs, iovs_cnt));
    if((ssize_t)write_len != ret)
    {
        xcc_util_write_format_safe(xc_crash_log_fd, XC_CRASH_ERR_TITLE"write args to pipe failed, return=%d, errno=%d\n\n", ret, errno);
        return 94;
    }

    //copy the read-side of the args-pipe to stdin (fd: 0)
    XCC_UTIL_TEMP_FAILURE_RETRY(dup2(pipefd[0], STDIN_FILENO));
    
    syscall(SYS_close, pipefd[0]);
    syscall(SYS_close, pipefd[1]);

    //escape to the dumper process
    errno = 0;
    execl(xc_crash_dumper_pathname, XCC_UTIL_XCRASH_DUMPER_FILENAME, NULL);
    return 100 + errno;
}

static void xc_xcrash_record_java_stacktrace()
{
    JNIEnv                           *env     = NULL;
    xc_dl_t                          *libcpp  = NULL;
    xc_dl_t                          *libart  = NULL;
    xcc_util_libart_thread_current_t  current = NULL;
    xcc_util_libart_thread_dump_t     dump    = NULL;
    xcc_util_libart_thread_dump2_t    dump2   = NULL;
    void                             *cerr    = NULL;
    void                             *thread  = NULL;

    //is this a java thread?
    if(JNI_OK == (*xc_common_vm)->GetEnv(xc_common_vm, (void**)&env, XC_JNI_VERSION))
        XC_JNI_CHECK_PENDING_EXCEPTION(end);
    else
        return;

    //yes, this is a java thread
    xc_crash_dump_java_stacktrace = 1;

    //in Dalvik, get java stacktrace on the java layer
    if(xc_common_api_level < 21) return;

    //peek libc++.so
    if(xc_common_api_level >= 29) libcpp = xc_dl_create(XCC_UTIL_LIBCPP_APEX);
    if(NULL == libcpp && NULL == (libcpp = xc_dl_create(XCC_UTIL_LIBCPP))) goto end;
    if(NULL == (cerr = xc_dl_sym(libcpp, XCC_UTIL_LIBCPP_CERR))) goto end;

    //peek libart.so
    if(xc_common_api_level >= 29) libart = xc_dl_create(XCC_UTIL_LIBART_APEX);
    if(NULL == libart && NULL == (libart = xc_dl_create(XCC_UTIL_LIBART))) goto end;
    if(NULL == (current = (xcc_util_libart_thread_current_t)xc_dl_sym(libart, XCC_UTIL_LIBART_THREAD_CURRENT))) goto end;
    if(NULL == (dump = (xcc_util_libart_thread_dump_t)xc_dl_sym(libart, XCC_UTIL_LIBART_THREAD_DUMP)))
    {
#ifndef __i386__
        if(NULL == (dump2 = (xcc_util_libart_thread_dump2_t)xc_dl_sym(libart, XCC_UTIL_LIBART_THREAD_DUMP2))) goto end;
#else
        goto end;
#endif
    }
    
    //get current thread object
    if(NULL == (thread = current())) goto end;

    //everything seems OK, do not dump java stacktrace again on the java layer
    xc_crash_dump_java_stacktrace = 0;

    //dump java stacktrace
    if(0 != xcc_util_write_str(xc_crash_log_fd, "\n\njava stacktrace:\n")) goto end;
    if(dup2(xc_crash_log_fd, STDERR_FILENO) < 0) goto end;
    if(NULL != dump)
        dump(thread, cerr);
    else if(NULL != dump2)
        dump2(thread, cerr, 0, 0);
    dup2(xc_common_fd_null, STDERR_FILENO);
    xcc_util_write_str(xc_crash_log_fd, "\n");

 end:
    if(NULL != libcpp) xc_dl_destroy(&libcpp);
    if(NULL != libart) xc_dl_destroy(&libart);
}

static void *xc_crash_callback_thread(void *arg)
{
    JNIEnv   *env = NULL;
    uint64_t  data = 0;
    jstring   j_pathname  = NULL;
    jstring   j_emergency = NULL;
    jboolean  j_dump_java_stacktrace = JNI_FALSE;
    jboolean  j_is_main_thread = JNI_FALSE;
    jstring   j_thread_name = NULL;
    char      c_thread_name[16] = "\0";
    
    (void)arg;
    
    JavaVMAttachArgs attach_args = {
        .version = XC_JNI_VERSION,
        .name    = "xcrash_crash_cb",
        .group   = NULL
    };
    if(JNI_OK != (*xc_common_vm)->AttachCurrentThread(xc_common_vm, &env, &attach_args)) return NULL;

    //block until native crashed
    if(sizeof(data) != XCC_UTIL_TEMP_FAILURE_RETRY(read(xc_crash_cb_notifier, &data, sizeof(data)))) goto end;

    //prepare callback parameters
    if(NULL == (j_pathname = (*env)->NewStringUTF(env, xc_crash_log_pathname))) goto end;
    if('\0' != xc_crash_emergency[0])
    {
        if(NULL == (j_emergency = (*env)->NewStringUTF(env, xc_crash_emergency))) goto end;
    }
    j_dump_java_stacktrace = (xc_crash_dump_java_stacktrace ? JNI_TRUE : JNI_FALSE);
    if(j_dump_java_stacktrace)
    {
        j_is_main_thread = (xc_common_process_id == xc_crash_tid ? JNI_TRUE : JNI_FALSE);
        if(!j_is_main_thread)
        {
            xcc_util_get_thread_name(xc_crash_tid, c_thread_name, sizeof(c_thread_name));
            if(NULL == (j_thread_name = (*env)->NewStringUTF(env, c_thread_name))) goto end;
        }
    }

    //do callback
    (*env)->CallStaticVoidMethod(env, xc_common_cb_class, xc_crash_cb_method, j_pathname, j_emergency,
                                 j_dump_java_stacktrace, j_is_main_thread, j_thread_name);
    XC_JNI_IGNORE_PENDING_EXCEPTION();

 end:
    (*xc_common_vm)->DetachCurrentThread(xc_common_vm);
    return NULL;
}

static void xc_crash_callback()
{
    uint64_t data;

    if(xc_crash_cb_notifier < 0 || NULL == xc_common_cb_class || NULL == xc_crash_cb_method) return;
    
    //wake up the callback thread
    data = 1;
    if(sizeof(data) != XCC_UTIL_TEMP_FAILURE_RETRY(write(xc_crash_cb_notifier, &data, sizeof(data)))) return;
    
    pthread_join(xc_crash_cb_thd, NULL);
}

static int xc_crash_check_backtrace_valid()
{
    int    fd;
    char   line[512];
    size_t i = 0;
    int    r = 0;
    
    if((fd = XCC_UTIL_TEMP_FAILURE_RETRY(open(xc_crash_log_pathname, O_RDONLY | O_CLOEXEC))) < 0)
    {
        if(xc_crash_prepared_fd >= 0)
        {
            close(xc_crash_prepared_fd);
            xc_crash_prepared_fd = -1;
        }
        if((fd = XCC_UTIL_TEMP_FAILURE_RETRY(open(xc_crash_log_pathname, O_RDONLY | O_CLOEXEC))) < 0)
            return 0; //failed
    }
    
    while(NULL != xcc_util_gets(line, sizeof(line), fd))
    {
        if(0 == memcmp(line, "backtrace:\n", 11))
        {
            //check the next line
            if(NULL != xcc_util_gets(line, sizeof(line), fd) && 0 == memcmp(line, "    #00 pc ", 11))
                r = 1; //we found the backtrace
            break;
        }
        if(i++ > 200) //check the top 200 lines at most
            break;
    }

    if(fd >= 0) close(fd);
    return r;    
}

static void xc_crash_signal_handler(int sig, siginfo_t *si, void *uc)
{
    struct timespec crash_tp;
    int             restore_orig_ptracer = 0;
    int             restore_orig_dumpable = 0;
    int             orig_dumpable = 0;
    int             dump_ok = 0;

    (void)sig;

    pthread_mutex_lock(&xc_crash_mutex);

    //only once
    if(xc_common_native_crashed) goto exit;
    xc_common_native_crashed = 1;

    //restore the original/default signal handler
    if(xc_crash_rethrow)
    {
        if(0 != xcc_signal_crash_unregister()) goto exit;
    }
    else
    {
        if(0 != xcc_signal_crash_ignore()) goto exit;
    }

    //save crash time
    clock_gettime(CLOCK_REALTIME, &crash_tp);
    xc_crash_time = (uint64_t)(crash_tp.tv_sec) * 1000 * 1000 + (uint64_t)crash_tp.tv_nsec / 1000;

    //save crashed thread ID
    xc_crash_tid = gettid();
    
    //create and open log file
    if((xc_crash_log_fd = xc_common_open_crash_log(xc_crash_log_pathname, sizeof(xc_crash_log_pathname), &xc_crash_log_from_placeholder)) < 0) goto end;

    //check privilege-restricting mode
    //https://www.kernel.org/doc/Documentation/prctl/no_new_privs.txt
    //errno = 0;
    //if(1 == prctl(PR_GET_NO_NEW_PRIVS, 0, 0, 0, 0))
    //{
    //    xcc_util_write_format_safe(xc_crash_log_fd, XC_CRASH_ERR_TITLE"get NO_NEW_PRIVS failed, errno=%d\n\n", errno);
    //    goto end;
    //}

    //set dumpable
    orig_dumpable = prctl(PR_GET_DUMPABLE);
    errno = 0;
    if(0 != prctl(PR_SET_DUMPABLE, 1))
    {
        xcc_util_write_format_safe(xc_crash_log_fd, XC_CRASH_ERR_TITLE"set dumpable failed, errno=%d\n\n", errno);
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
            xcc_util_write_format_safe(xc_crash_log_fd, XC_CRASH_ERR_TITLE"set traceable failed, errno=%d\n\n", errno);
            goto end;
        }
    }
    else
    {
        restore_orig_ptracer = 1;
    }

    //set crash spot info
    xc_crash_spot.crash_time = xc_crash_time;
    xc_crash_spot.crash_tid = xc_crash_tid;
    memcpy(&(xc_crash_spot.siginfo), si, sizeof(siginfo_t));
    memcpy(&(xc_crash_spot.ucontext), uc, sizeof(ucontext_t));
    xc_crash_spot.log_pathname_len = strlen(xc_crash_log_pathname);

    //spawn crash dumper process
    errno = 0;
    pid_t dumper_pid = xc_crash_fork(xc_crash_exec_dumper);
    if(-1 == dumper_pid)
    {
        xcc_util_write_format_safe(xc_crash_log_fd, XC_CRASH_ERR_TITLE"fork failed, errno=%d\n\n", errno);
        goto end;
    }

    //parent process ...

    //wait the crash dumper process terminated
    errno = 0;
    int status = 0;
    int wait_r = XCC_UTIL_TEMP_FAILURE_RETRY(waitpid(dumper_pid, &status, __WALL));

    //the crash dumper process should have written a lot of logs,
    //so we need to seek to the end of log file
    if(xc_crash_log_from_placeholder)
    {
        if((xc_crash_log_fd = xc_common_seek_to_content_end(xc_crash_log_fd)) < 0) goto end;
    }
    
    if(-1 == wait_r)
    {
        xcc_util_write_format_safe(xc_crash_log_fd, XC_CRASH_ERR_TITLE"waitpid failed, errno=%d\n\n", errno);
        goto end;
    }

    //check child process state
    if(!(WIFEXITED(status)) || 0 != WEXITSTATUS(status))
    {
        if(WIFEXITED(status) && 0 != WEXITSTATUS(status))
        {
            //terminated normally, but return / exit / _exit NON-zero
            xcc_util_write_format_safe(xc_crash_log_fd, XC_CRASH_ERR_TITLE"child terminated normally with non-zero exit status(%d), dumper=%s\n\n", WEXITSTATUS(status), xc_crash_dumper_pathname);
            goto end;
        }
        else if(WIFSIGNALED(status))
        {
            //terminated by a signal
            xcc_util_write_format_safe(xc_crash_log_fd, XC_CRASH_ERR_TITLE"child terminated by a signal(%d)\n\n", WTERMSIG(status));
            goto end;
        }
        else
        {
            xcc_util_write_format_safe(xc_crash_log_fd, XC_CRASH_ERR_TITLE"child terminated with other error status(%d), dumper=%s\n\n", status, xc_crash_dumper_pathname);
            goto end;
        }
    }

    //check the backtrace
    if(!xc_crash_check_backtrace_valid()) goto end;
    
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
                                  xc_crash_tid,
                                  xc_crash_time,
                                  xc_crash_emergency,
                                  XC_CRASH_EMERGENCY_BUF_LEN);
        
        if(xc_crash_log_fd >= 0)
        {
            if(0 != xc_fallback_record(xc_crash_log_fd,
                                       xc_crash_emergency,
                                       xc_crash_spot.logcat_system_lines,
                                       xc_crash_spot.logcat_events_lines,
                                       xc_crash_spot.logcat_main_lines,
                                       xc_crash_spot.dump_fds))
            {
                close(xc_crash_log_fd);
                xc_crash_log_fd = -1;
            }
        }
    }

    if(xc_crash_log_fd >= 0)
    {
        //record java stacktrace
        xc_xcrash_record_java_stacktrace();
        
        //we have written all the required information in the native layer, close the FD
        close(xc_crash_log_fd);
        xc_crash_log_fd = -1;
    }

    //JNI callback
    xc_crash_callback();

    if(0 != xcc_signal_crash_queue(si)) goto exit;
    
    pthread_mutex_unlock(&xc_crash_mutex);
    return;

 exit:
    pthread_mutex_unlock(&xc_crash_mutex);
    _exit(1);
}

static void xc_crash_init_dump_all_threads_whitelist(const char **whitelist, size_t whitelist_len)
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

    xc_crash_spot.dump_all_threads_whitelist_len = cur_encoded_len;
    xc_crash_dump_all_threads_whitelist = total_encoded_whitelist;
}

static void xc_crash_init_callback(JNIEnv *env)
{
    if(NULL == xc_common_cb_class) return;
    
    xc_crash_cb_method = (*env)->GetStaticMethodID(env, xc_common_cb_class, XC_CRASH_CALLBACK_METHOD_NAME, XC_CRASH_CALLBACK_METHOD_SIGNATURE);
    XC_JNI_CHECK_NULL_AND_PENDING_EXCEPTION(xc_crash_cb_method, err);
    
    //eventfd and a new thread for callback
    if(0 > (xc_crash_cb_notifier = eventfd(0, EFD_CLOEXEC))) goto err;
    if(0 != pthread_create(&xc_crash_cb_thd, NULL, xc_crash_callback_thread, NULL)) goto err;
    return;

 err:
    xc_crash_cb_method = NULL;
    if(xc_crash_cb_notifier >= 0)
    {
        close(xc_crash_cb_notifier);
        xc_crash_cb_notifier = -1;
    }
}

int xc_crash_init(JNIEnv *env,
                  int rethrow,
                  unsigned int logcat_system_lines,
                  unsigned int logcat_events_lines,
                  unsigned int logcat_main_lines,
                  int dump_elf_hash,
                  int dump_map,
                  int dump_fds,
                  int dump_all_threads,
                  unsigned int dump_all_threads_count_max,
                  const char **dump_all_threads_whitelist,
                  size_t dump_all_threads_whitelist_len)
{
    xc_crash_prepared_fd = XCC_UTIL_TEMP_FAILURE_RETRY(open("/dev/null", O_RDWR));
    xc_crash_rethrow = rethrow;
    if(NULL == (xc_crash_emergency = calloc(XC_CRASH_EMERGENCY_BUF_LEN, 1))) return XCC_ERRNO_NOMEM;
    if(NULL == (xc_crash_dumper_pathname = xc_util_strdupcat(xc_common_app_lib_dir, "/"XCC_UTIL_XCRASH_DUMPER_FILENAME))) return XCC_ERRNO_NOMEM;

    //init the local unwinder for fallback mode
    xcc_unwind_init(xc_common_api_level);

    //init for JNI callback
    xc_crash_init_callback(env);

    //struct info passed to the dumper process
    memset(&xc_crash_spot, 0, sizeof(xcc_spot_t));
    xc_crash_spot.api_level = xc_common_api_level;
    xc_crash_spot.crash_pid = xc_common_process_id;
    xc_crash_spot.start_time = xc_common_start_time;
    xc_crash_spot.time_zone = xc_common_time_zone;
    xc_crash_spot.logcat_system_lines = logcat_system_lines;
    xc_crash_spot.logcat_events_lines = logcat_events_lines;
    xc_crash_spot.logcat_main_lines = logcat_main_lines;
    xc_crash_spot.dump_elf_hash = dump_elf_hash;
    xc_crash_spot.dump_map = dump_map;
    xc_crash_spot.dump_fds = dump_fds;
    xc_crash_spot.dump_all_threads = dump_all_threads;
    xc_crash_spot.dump_all_threads_count_max = dump_all_threads_count_max;
    xc_crash_spot.os_version_len = strlen(xc_common_os_version);
    xc_crash_spot.kernel_version_len = strlen(xc_common_kernel_version);
    xc_crash_spot.abi_list_len = strlen(xc_common_abi_list);
    xc_crash_spot.manufacturer_len = strlen(xc_common_manufacturer);
    xc_crash_spot.brand_len = strlen(xc_common_brand);
    xc_crash_spot.model_len = strlen(xc_common_model);
    xc_crash_spot.build_fingerprint_len = strlen(xc_common_build_fingerprint);
    xc_crash_spot.app_id_len = strlen(xc_common_app_id);
    xc_crash_spot.app_version_len = strlen(xc_common_app_version);
    xc_crash_init_dump_all_threads_whitelist(dump_all_threads_whitelist, dump_all_threads_whitelist_len);

    //for clone and fork
#ifndef __i386__
    if(NULL == (xc_crash_child_stack = calloc(XC_CRASH_CHILD_STACK_LEN, 1))) return XCC_ERRNO_NOMEM;
    xc_crash_child_stack = (void *)(((uint8_t *)xc_crash_child_stack) + XC_CRASH_CHILD_STACK_LEN);
#else
    if(0 != pipe2(xc_crash_child_notifier, O_CLOEXEC)) return XCC_ERRNO_SYS;
#endif
    
    //register signal handler
    return xcc_signal_crash_register(xc_crash_signal_handler);
}

#pragma clang diagnostic pop
