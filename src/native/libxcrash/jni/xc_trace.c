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

// Created by caikelun on 2019-08-13.

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <inttypes.h>
#include <errno.h>
#include <signal.h>
#include <dirent.h>
#include <sys/eventfd.h>
#include <sys/syscall.h>
#include <android/log.h>
#include "xcc_errno.h"
#include "xcc_util.h"
#include "xcc_signal.h"
#include "xcc_meminfo.h"
#include "xcc_version.h"
#include "xc_trace.h"
#include "xc_common.h"
#include "xc_dl.h"
#include "xc_jni.h"
#include "xc_util.h"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wgnu-statement-expression"

#define XC_TRACE_CALLBACK_METHOD_NAME         "traceCallback"
#define XC_TRACE_CALLBACK_METHOD_SIGNATURE    "(Ljava/lang/String;Ljava/lang/String;)V"

#define XC_TRACE_SIGNAL_CATCHER_TID_UNLOAD    (-2)
#define XC_TRACE_SIGNAL_CATCHER_TID_UNKNOWN   (-1)
#define XC_TRACE_SIGNAL_CATCHER_THREAD_NAME   "Signal Catcher"
#define XC_TRACE_SIGNAL_CATCHER_THREAD_SIGBLK 0x1000

static int                              xc_trace_is_lollipop = 0;
static pid_t                            xc_trace_signal_catcher_tid = XC_TRACE_SIGNAL_CATCHER_TID_UNLOAD;

//symbol address in libc++.so and libart.so
static void                            *xc_trace_libcpp_cerr = NULL;
static void                           **xc_trace_libart_runtime_instance = NULL;
static xcc_util_libart_runtime_dump_t   xc_trace_libart_runtime_dump = NULL;
static xcc_util_libart_dbg_suspend_t    xc_trace_libart_dbg_suspend = NULL;
static xcc_util_libart_dbg_resume_t     xc_trace_libart_dbg_resume = NULL;
static int                              xc_trace_symbols_loaded = 0;
static int                              xc_trace_symbols_status = XCC_ERRNO_NOTFND;

//init parameters
static int                              xc_trace_rethrow;
static unsigned int                     xc_trace_logcat_system_lines;
static unsigned int                     xc_trace_logcat_events_lines;
static unsigned int                     xc_trace_logcat_main_lines;
static int                              xc_trace_dump_fds;

//callback
static jmethodID                        xc_trace_cb_method = NULL;
static int                              xc_trace_notifier = -1;

static void xc_trace_load_signal_catcher_tid()
{
    char           buf[256];
    DIR           *dir;
    struct dirent *ent;
    FILE          *f;
    pid_t          tid;
    uint64_t       sigblk;

    xc_trace_signal_catcher_tid = XC_TRACE_SIGNAL_CATCHER_TID_UNKNOWN;

    snprintf(buf, sizeof(buf), "/proc/%d/task", xc_common_process_id);
    if(NULL == (dir = opendir(buf))) return;
    while(NULL != (ent = readdir(dir)))
    {
        //get and check thread id
        if(0 != xcc_util_atoi(ent->d_name, &tid)) continue;
        if(tid < 0) continue;

        //check thread name
        xcc_util_get_thread_name(tid, buf, sizeof(buf));
        if(0 != strcmp(buf, XC_TRACE_SIGNAL_CATCHER_THREAD_NAME)) continue;

        //check signal block masks
        sigblk = 0;
        snprintf(buf, sizeof(buf), "/proc/%d/status", tid);
        if(NULL == (f = fopen(buf, "r"))) break;
        while(fgets(buf, sizeof(buf), f))
        {
            if(1 == sscanf(buf, "SigBlk: %"SCNx64, &sigblk)) break;
        }
        fclose(f);
        if(XC_TRACE_SIGNAL_CATCHER_THREAD_SIGBLK != sigblk) continue;

        //found it
        xc_trace_signal_catcher_tid = tid;
        break;
    }
    closedir(dir);
}

static void xc_trace_send_sigquit()
{
    if(XC_TRACE_SIGNAL_CATCHER_TID_UNLOAD == xc_trace_signal_catcher_tid)
        xc_trace_load_signal_catcher_tid();

    if(xc_trace_signal_catcher_tid >= 0)
        syscall(SYS_tgkill, xc_common_process_id, xc_trace_signal_catcher_tid, SIGQUIT);
}

static int xc_trace_load_symbols()
{
    xc_dl_t *libcpp = NULL;
    xc_dl_t *libart = NULL;

    //only once
    if(xc_trace_symbols_loaded) return xc_trace_symbols_status;
    xc_trace_symbols_loaded = 1;

    if(xc_common_api_level >= 29) libcpp = xc_dl_create(XCC_UTIL_LIBCPP_APEX);
    if(NULL == libcpp && NULL == (libcpp = xc_dl_create(XCC_UTIL_LIBCPP))) goto end;
    if(NULL == (xc_trace_libcpp_cerr = xc_dl_sym(libcpp, XCC_UTIL_LIBCPP_CERR))) goto end;

    if(xc_common_api_level >= 29) libart = xc_dl_create(XCC_UTIL_LIBART_APEX);
    if(NULL == libart && NULL == (libart = xc_dl_create(XCC_UTIL_LIBART))) goto end;
    if(NULL == (xc_trace_libart_runtime_instance = (void **)xc_dl_sym(libart, XCC_UTIL_LIBART_RUNTIME_INSTANCE))) goto end;
    if(NULL == (xc_trace_libart_runtime_dump = (xcc_util_libart_runtime_dump_t)xc_dl_sym(libart, XCC_UTIL_LIBART_RUNTIME_DUMP))) goto end;
    if(xc_trace_is_lollipop)
    {
        if(NULL == (xc_trace_libart_dbg_suspend = (xcc_util_libart_dbg_suspend_t)xc_dl_sym(libart, XCC_UTIL_LIBART_DBG_SUSPEND))) goto end;
        if(NULL == (xc_trace_libart_dbg_resume = (xcc_util_libart_dbg_resume_t)xc_dl_sym(libart, XCC_UTIL_LIBART_DBG_RESUME))) goto end;
    }

    //OK
    xc_trace_symbols_status = 0;

 end:
    if(NULL != libcpp) xc_dl_destroy(&libcpp);
    if(NULL != libart) xc_dl_destroy(&libart);
    return xc_trace_symbols_status;
}

//Not reliable! But try our best to avoid crashes.
static int xc_trace_check_address_valid()
{
    FILE      *f = NULL;
    char       line[512];
    uintptr_t  start, end;
    int        r_cerr = XCC_ERRNO_INVAL;
    int        r_runtime_instance = XCC_ERRNO_INVAL;
    int        r_runtime_dump = XCC_ERRNO_INVAL;
    int        r_dbg_suspend = XCC_ERRNO_INVAL;
    int        r_dbg_resume = XCC_ERRNO_INVAL;
    int        r = XCC_ERRNO_INVAL;

    if(NULL == (f = fopen("/proc/self/maps", "r"))) return XCC_ERRNO_SYS;
    
    while(fgets(line, sizeof(line), f))
    {
        if(2 != sscanf(line, "%"SCNxPTR"-%"SCNxPTR" r", &start, &end)) continue;
        
        if(0 != r_cerr && (uintptr_t)xc_trace_libcpp_cerr >= start && (uintptr_t)xc_trace_libcpp_cerr < end)
            r_cerr = 0;
        if(0 != r_runtime_instance && (uintptr_t)xc_trace_libart_runtime_instance >= start && (uintptr_t)xc_trace_libart_runtime_instance < end)
            r_runtime_instance = 0;
        if(0 != r_runtime_dump && (uintptr_t)xc_trace_libart_runtime_dump >= start && (uintptr_t)xc_trace_libart_runtime_dump < end)
            r_runtime_dump = 0;
        if(xc_trace_is_lollipop)
        {
            if(0 != r_dbg_suspend && (uintptr_t)xc_trace_libart_dbg_suspend >= start && (uintptr_t)xc_trace_libart_dbg_suspend < end)
                r_dbg_suspend = 0;
            if(0 != r_dbg_resume && (uintptr_t)xc_trace_libart_dbg_resume >= start && (uintptr_t)xc_trace_libart_dbg_resume < end)
                r_dbg_resume = 0;
        }
        
        if(0 == r_cerr && 0 == r_runtime_instance && 0 == r_runtime_dump &&
           (!xc_trace_is_lollipop || (0 == r_dbg_suspend && 0 == r_dbg_resume)))
        {
            r = 0;
            break;
        }
    }
    if(0 != r) goto end;

    r = XCC_ERRNO_INVAL;
    rewind(f);
    while(fgets(line, sizeof(line), f))
    {
        if(2 != sscanf(line, "%"SCNxPTR"-%"SCNxPTR" r", &start, &end)) continue;

        //The next line of code will cause segmentation fault, sometimes.
        if((uintptr_t)(*xc_trace_libart_runtime_instance) >= start && (uintptr_t)(*xc_trace_libart_runtime_instance) < end)
        {
            r = 0;
            break;
        }
    }
    
 end:
    fclose(f);
    return r;
}

static int xc_trace_logs_filter(const struct dirent *entry)
{
    size_t len;
    
    if(DT_REG != entry->d_type) return 0;

    len = strlen(entry->d_name);
    if(len < XC_COMMON_LOG_NAME_MIN_TRACE) return 0;
    
    if(0 != memcmp(entry->d_name, XC_COMMON_LOG_PREFIX"_", XC_COMMON_LOG_PREFIX_LEN + 1)) return 0;
    if(0 != memcmp(entry->d_name + (len - XC_COMMON_LOG_SUFFIX_TRACE_LEN), XC_COMMON_LOG_SUFFIX_TRACE, XC_COMMON_LOG_SUFFIX_TRACE_LEN)) return 0;

    return 1;
}

static int xc_trace_logs_clean(void)
{
    struct dirent **entry_list;
    char            pathname[1024];
    int             n, i, r = 0;

    if(0 > (n = scandir(xc_common_log_dir, &entry_list, xc_trace_logs_filter, alphasort))) return XCC_ERRNO_SYS;
    for(i = 0; i < n; i++)
    {
        snprintf(pathname, sizeof(pathname), "%s/%s", xc_common_log_dir, entry_list[i]->d_name);
        if(0 != unlink(pathname)) r = XCC_ERRNO_SYS;
    }
    free(entry_list);
    return r;
}

static int xc_trace_write_header(int fd, uint64_t trace_time)
{
    int  r;
    char buf[1024];
    
    xcc_util_get_dump_header(buf, sizeof(buf),
                             XCC_UTIL_CRASH_TYPE_ANR,
                             xc_common_time_zone,
                             xc_common_start_time,
                             trace_time,
                             xc_common_app_id,
                             xc_common_app_version,
                             xc_common_api_level,
                             xc_common_os_version,
                             xc_common_kernel_version,
                             xc_common_abi_list,
                             xc_common_manufacturer,
                             xc_common_brand,
                             xc_common_model,
                             xc_common_build_fingerprint);
    if(0 != (r = xcc_util_write_str(fd, buf))) return r;
    
    return xcc_util_write_format(fd, "pid: %d  >>> %s <<<\n\n", xc_common_process_id, xc_common_process_name);
}

static void *xc_trace_dumper(void *arg)
{
    JNIEnv         *env = NULL;
    uint64_t        data;
    uint64_t        trace_time;
    int             fd;
    struct timeval  tv;
    char            pathname[1024];
    jstring         j_pathname;
    
    (void)arg;
    
    pthread_detach(pthread_self());

    JavaVMAttachArgs attach_args = {
        .version = XC_JNI_VERSION,
        .name    = "xcrash_trace_dp",
        .group   = NULL
    };
    if(JNI_OK != (*xc_common_vm)->AttachCurrentThread(xc_common_vm, &env, &attach_args)) goto exit;

    while(1)
    {
        //block here, waiting for sigquit
        XCC_UTIL_TEMP_FAILURE_RETRY(read(xc_trace_notifier, &data, sizeof(data)));
        
        //check if process already crashed
        if(xc_common_native_crashed || xc_common_java_crashed) break;

        //trace time
        if(0 != gettimeofday(&tv, NULL)) break;
        trace_time = (uint64_t)(tv.tv_sec) * 1000 * 1000 + (uint64_t)tv.tv_usec;

        //Keep only one current trace.
        if(0 != xc_trace_logs_clean()) continue;

        //create and open log file
        if((fd = xc_common_open_trace_log(pathname, sizeof(pathname), trace_time)) < 0) continue;

        //write header info
        if(0 != xc_trace_write_header(fd, trace_time)) goto end;

        //write trace info from ART runtime
        if(0 != xcc_util_write_format(fd, XCC_UTIL_THREAD_SEP"Cmd line: %s\n", xc_common_process_name)) goto end;
        if(0 != xcc_util_write_str(fd, "Mode: ART DumpForSigQuit\n")) goto end;
        if(0 != xc_trace_load_symbols())
        {
            if(0 != xcc_util_write_str(fd, "Failed to load symbols.\n")) goto end;
            goto skip;
        }
        if(0 != xc_trace_check_address_valid())
        {
            if(0 != xcc_util_write_str(fd, "Failed to check runtime address.\n")) goto end;
            goto skip;
        }
        if(dup2(fd, STDERR_FILENO) < 0)
        {
            if(0 != xcc_util_write_str(fd, "Failed to duplicate FD.\n")) goto end;
            goto skip;
        }
        if(xc_trace_is_lollipop)
            xc_trace_libart_dbg_suspend();
        xc_trace_libart_runtime_dump(*xc_trace_libart_runtime_instance, xc_trace_libcpp_cerr);
        if(xc_trace_is_lollipop)
            xc_trace_libart_dbg_resume();
        dup2(xc_common_fd_null, STDERR_FILENO);
                            
    skip:
        if(0 != xcc_util_write_str(fd, "\n"XCC_UTIL_THREAD_END"\n")) goto end;

        //write other info
        if(0 != xcc_util_record_logcat(fd, xc_common_process_id, xc_common_api_level, xc_trace_logcat_system_lines, xc_trace_logcat_events_lines, xc_trace_logcat_main_lines)) goto end;
        if(xc_trace_dump_fds)
            if(0 != xcc_util_record_fds(fd, xc_common_process_id)) goto end;
        if(0 != xcc_meminfo_record(fd, xc_common_process_id)) goto end;

    end:
        //close log file
        xc_common_close_trace_log(fd);

        //rethrow SIGQUIT to ART Signal Catcher
        if(xc_trace_rethrow) xc_trace_send_sigquit();

        //JNI callback
        //Do we need to implement an emergency buffer for disk exhausted?
        if(NULL == xc_trace_cb_method) continue;
        if(NULL == (j_pathname = (*env)->NewStringUTF(env, pathname))) continue;
        (*env)->CallStaticVoidMethod(env, xc_common_cb_class, xc_trace_cb_method, j_pathname, NULL);
        XC_JNI_IGNORE_PENDING_EXCEPTION();
        (*env)->DeleteLocalRef(env, j_pathname);
    }
    
    (*xc_common_vm)->DetachCurrentThread(xc_common_vm);

 exit:
    xc_trace_notifier = -1;
    close(xc_trace_notifier);
    return NULL;
}

static void xc_trace_handler(int sig, siginfo_t *si, void *uc)
{
    uint64_t data;
    
    (void)sig;
    (void)si;
    (void)uc;

    if(xc_trace_notifier >= 0)
    {
        data = 1;
        XCC_UTIL_TEMP_FAILURE_RETRY(write(xc_trace_notifier, &data, sizeof(data)));
    }
}

static void xc_trace_init_callback(JNIEnv *env)
{
    if(NULL == xc_common_cb_class) return;
    
    xc_trace_cb_method = (*env)->GetStaticMethodID(env, xc_common_cb_class, XC_TRACE_CALLBACK_METHOD_NAME, XC_TRACE_CALLBACK_METHOD_SIGNATURE);
    XC_JNI_CHECK_NULL_AND_PENDING_EXCEPTION(xc_trace_cb_method, err);
    return;

 err:
    xc_trace_cb_method = NULL;
}

int xc_trace_init(JNIEnv *env,
                int rethrow,
                unsigned int logcat_system_lines,
                unsigned int logcat_events_lines,
                unsigned int logcat_main_lines,
                int dump_fds)
{
    int r;
    pthread_t thd;

    //capture SIGQUIT only for ART
    if(xc_common_api_level < 21) return 0;

    //is Android Lollipop (5.x)?
    xc_trace_is_lollipop = ((21 == xc_common_api_level || 22 == xc_common_api_level) ? 1 : 0);

    xc_trace_rethrow = rethrow;
    xc_trace_logcat_system_lines = logcat_system_lines;
    xc_trace_logcat_events_lines = logcat_events_lines;
    xc_trace_logcat_main_lines = logcat_main_lines;
    xc_trace_dump_fds = dump_fds;

    //init for JNI callback
    xc_trace_init_callback(env);

    //create event FD
    if(0 > (xc_trace_notifier = eventfd(0, EFD_CLOEXEC))) return XCC_ERRNO_SYS;

    //register signal handler
    if(0 != (r = xcc_signal_trace_register(xc_trace_handler))) goto err2;

    //create thread for dump trace
    if(0 != (r = pthread_create(&thd, NULL, xc_trace_dumper, NULL))) goto err1;

    return 0;

 err1:
    xcc_signal_trace_unregister();
 err2:
    close(xc_trace_notifier);
    xc_trace_notifier = -1;
    
    return r;
}

#pragma clang diagnostic pop
