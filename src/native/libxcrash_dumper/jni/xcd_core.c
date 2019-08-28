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

#include <inttypes.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <time.h>
#include <ucontext.h>
#include <pthread.h>
#include <signal.h>
#include <dirent.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <linux/elf.h>
#include <android/log.h>
#include "queue.h"
#include "xcc_errno.h"
#include "xcc_signal.h"
#include "xcc_unwind.h"
#include "xcc_util.h"
#include "xcc_spot.h"
#include "xcd_log.h"
#include "xcd_process.h"
#include "xcd_sys.h"
#include "xcd_util.h"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wgnu-statement-expression"

static int                    xcd_core_handled      = 0;
static int                    xcd_core_log_fd       = -1;
static xcd_process_t         *xcd_core_proc         = NULL;

static xcc_spot_t             xcd_core_spot;
static char                  *xcd_core_log_pathname      = NULL;
static char                  *xcd_core_os_version        = NULL;
static char                  *xcd_core_kernel_version    = NULL;
static char                  *xcd_core_abi_list          = NULL;
static char                  *xcd_core_manufacturer      = NULL;
static char                  *xcd_core_brand             = NULL;
static char                  *xcd_core_model             = NULL;
static char                  *xcd_core_build_fingerprint = NULL;
static char                  *xcd_core_app_id            = NULL;
static char                  *xcd_core_app_version       = NULL;
static char                  *xcd_core_dump_all_threads_whitelist = NULL;

static int xcd_core_read_stdin(void *buf, size_t len)
{
    size_t  nread = 0;
    ssize_t n;

    while(len - nread > 0)
    {
        n = XCC_UTIL_TEMP_FAILURE_RETRY(read(STDIN_FILENO, (void *)((uint8_t *)buf + nread), len - nread));
        if(n <= 0) return XCC_ERRNO_SYS;
        nread += (size_t)n;
    }
    
    return 0;
}

static int xcd_core_read_stdin_extra(char **buf, size_t len)
{
    if(0 == len) return XCC_ERRNO_INVAL;
    if(NULL == ((*buf) = (char *)calloc(1, len + 1))) return XCC_ERRNO_NOMEM;
    return xcd_core_read_stdin((void *)(*buf), len);
}

static int xcd_core_read_args()
{
    int r;
    
    if(0 != (r = xcd_core_read_stdin((void *)&xcd_core_spot, sizeof(xcc_spot_t)))) return r;
    if(0 != (r = xcd_core_read_stdin_extra(&xcd_core_log_pathname, xcd_core_spot.log_pathname_len))) return r;
    if(0 != (r = xcd_core_read_stdin_extra(&xcd_core_os_version, xcd_core_spot.os_version_len))) return r;
    if(0 != (r = xcd_core_read_stdin_extra(&xcd_core_kernel_version, xcd_core_spot.kernel_version_len))) return r;
    if(0 != (r = xcd_core_read_stdin_extra(&xcd_core_abi_list, xcd_core_spot.abi_list_len))) return r;
    if(0 != (r = xcd_core_read_stdin_extra(&xcd_core_manufacturer, xcd_core_spot.manufacturer_len))) return r;
    if(0 != (r = xcd_core_read_stdin_extra(&xcd_core_brand, xcd_core_spot.brand_len))) return r;
    if(0 != (r = xcd_core_read_stdin_extra(&xcd_core_model, xcd_core_spot.model_len))) return r;
    if(0 != (r = xcd_core_read_stdin_extra(&xcd_core_build_fingerprint, xcd_core_spot.build_fingerprint_len))) return r;
    if(0 != (r = xcd_core_read_stdin_extra(&xcd_core_app_id, xcd_core_spot.app_id_len))) return r;
    if(0 != (r = xcd_core_read_stdin_extra(&xcd_core_app_version, xcd_core_spot.app_version_len))) return r;
    if(xcd_core_spot.dump_all_threads_whitelist_len > 0)
        if(0 != (r = xcd_core_read_stdin_extra(&xcd_core_dump_all_threads_whitelist, xcd_core_spot.dump_all_threads_whitelist_len))) return r;
    
    return 0;
}

static void xcd_core_signal_handler(int sig, siginfo_t *si, void *uc)
{
    char buf[2048] = "\0";
    size_t len;

    (void)sig;

    //only once
    if(xcd_core_handled) _exit(200);
    xcd_core_handled = 1;

    //restore the signal handler
    if(0 != xcc_signal_crash_unregister()) _exit(10);

    if(xcd_core_log_fd >= 0)
    {
        //dump signal, code, backtrace
        if(0 != xcc_util_write_format_safe(xcd_core_log_fd,
                                           "\n\n"
                                           "xcrash error debug:\n"
                                           "dumper has crashed (signal: %d, code: %d)\n",
                                           si->si_signo, si->si_code)) goto end;
        if(0 < (len = xcc_unwind_get(xcd_core_spot.api_level, si, uc, buf, sizeof(buf))))
            xcc_util_write(xcd_core_log_fd, buf, len);

    end:
        xcc_util_write_str(xcd_core_log_fd, "\n\n");
    }

    xcc_signal_crash_queue(si);
}

int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;
    
    //don't leave a zombie process
    alarm(30);

    //read args from stdin
    if(0 != xcd_core_read_args()) exit(1);

    //open log file
    if(0 > (xcd_core_log_fd = XCC_UTIL_TEMP_FAILURE_RETRY(open(xcd_core_log_pathname, O_WRONLY | O_CLOEXEC)))) exit(2);

    //register signal handler for catching self-crashing
    xcc_unwind_init(xcd_core_spot.api_level);
    xcc_signal_crash_register(xcd_core_signal_handler);

    //create process object
    if(0 != xcd_process_create(&xcd_core_proc,
                               xcd_core_spot.crash_pid,
                               xcd_core_spot.crash_tid,
                               &(xcd_core_spot.siginfo),
                               &(xcd_core_spot.ucontext))) exit(3);

    //suspend all threads in the process
    xcd_process_suspend_threads(xcd_core_proc);

    //load process info
    if(0 != xcd_process_load_info(xcd_core_proc)) exit(4);

    //record system info
    if(0 != xcd_sys_record(xcd_core_log_fd,
                           xcd_core_spot.time_zone,
                           xcd_core_spot.start_time,
                           xcd_core_spot.crash_time,
                           xcd_core_app_id,
                           xcd_core_app_version,
                           xcd_core_spot.api_level,
                           xcd_core_os_version,
                           xcd_core_kernel_version,
                           xcd_core_abi_list,
                           xcd_core_manufacturer,
                           xcd_core_brand,
                           xcd_core_model,
                           xcd_core_build_fingerprint)) exit(5);

    //record process info
    if(0 != xcd_process_record(xcd_core_proc,
                               xcd_core_log_fd,
                               xcd_core_spot.logcat_system_lines,
                               xcd_core_spot.logcat_events_lines,
                               xcd_core_spot.logcat_main_lines,
                               xcd_core_spot.dump_elf_hash,
                               xcd_core_spot.dump_map,
                               xcd_core_spot.dump_fds,
                               xcd_core_spot.dump_all_threads,
                               xcd_core_spot.dump_all_threads_count_max,
                               xcd_core_dump_all_threads_whitelist,
                               xcd_core_spot.api_level)) exit(6);

    //resume all threads in the process
    xcd_process_resume_threads(xcd_core_proc);

#if XCD_CORE_DEBUG
    XCD_LOG_DEBUG("CORE: done");
#endif
    return 0;
}

#pragma clang diagnostic pop
