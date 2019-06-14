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
#include <signal.h>
#include <ucontext.h>
#include <unwind.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include "xcc_errno.h"
#include "xcc_util.h"
#include "xcc_unwind.h"
#include "xcc_fmt.h"
#include "xcc_version.h"
#include "xc_fallback.h"
#include "xc_util.h"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wgnu-statement-expression"

#define XC_FALLBACK_TIME_FORMAT "%s: '%04d-%02d-%02dT%02d:%02d:%02d.%03ld%c%02ld%02ld'\n"

static size_t xc_fallback_get_file_line(char *buf, size_t len, const char *title, const char *path)
{
    int   fd2 = -1;
    char  line[256];
    char *p = "unknown";
    
    if((fd2 = open(path, O_RDONLY)) < 0) goto end;
    if(NULL == xcc_util_gets(line, sizeof(line), fd2)) goto end;
    p = xcc_util_trim(line);

 end:
    if(fd2 >= 0) close(fd2);
    return xcc_fmt_snprintf(buf, len, "%s: '%s'\n", title, p);
}

static size_t xc_fallback_get_cpu(char *buf, size_t len)
{
    size_t used = 0;

    used += xc_fallback_get_file_line(buf + used, len - used, "CPU loadavg", "/proc/loadavg");
    used += xc_fallback_get_file_line(buf + used, len - used, "CPU online",  "/sys/devices/system/cpu/online");
    used += xc_fallback_get_file_line(buf + used, len - used, "CPU offline", "/sys/devices/system/cpu/offline");

    return used;
}

static int xc_fallback_parse_kb(char *line, const char *key)
{
    size_t line_len;
    size_t key_len;
    int    val = -1;

    line = xcc_util_trim(line);
    line_len = strlen(line);
    key_len = strlen(key);
    if(line_len < key_len + 3 + 1) return -1;

    if(0 != memcmp(line, key, key_len) || 0 != memcmp(line + line_len - 3, " kB", 3)) return -1;
    
    line[line_len - 3] = '\0';
    line = xcc_util_trim(line + key_len);
    if(0 != xcc_util_atoi(line, &val)) return -1;

    return val;
}

static size_t xc_fallback_get_system_mem(char *buf, size_t len)
{
    int     fd = -1;
    char    line[256];
    int     val;
    size_t  mtotal = 0;
    size_t  mfree = 0;
    size_t  mbuffers = 0;
    size_t  mcached = 0;
    size_t  mfree_all = 0;
    size_t  used = 0;

    if((fd = XCC_UTIL_TEMP_FAILURE_RETRY(open("/proc/meminfo", O_RDONLY | O_CLOEXEC))) < 0) goto end;
    
    while(NULL != xcc_util_gets(line, sizeof(line), fd))
    {
        if((val = xc_fallback_parse_kb(line, "MemTotal:")) >= 0)
            mtotal = (size_t)val;
        else if((val = xc_fallback_parse_kb(line, "MemFree:")) >= 0)
            mfree = (size_t)val;
        else if((val = xc_fallback_parse_kb(line, "Buffers:")) >= 0)
            mbuffers = (size_t)val;
        else if((val = xc_fallback_parse_kb(line, "Cached:")) >= 0)
            mcached = (size_t)val;
    }
    mfree_all = mfree + mbuffers + mcached;
    if(mfree_all > mtotal)
    {
        //something wrong
        mtotal = 0;
        mfree_all = 0;
    }
    
 end:
    if(fd >= 0) close(fd);
    used += xcc_fmt_snprintf(buf + used, len - used, "System memory total: '%zu kB'\n", mtotal);
    used += xcc_fmt_snprintf(buf + used, len - used, "System memory used: '%zu kB'\n", mtotal - mfree_all);
    return used;
}

static size_t xc_fallback_get_number_of_threads(pid_t pid)
{
    int               fd = -1;
    char              path[64];
    char              buf[512];
    long              n, i;
    int               tid;
    size_t            total = 0;
    xc_util_dirent_t *ent;
    
    xcc_fmt_snprintf(path, sizeof(path), "/proc/%d/task", pid);
    if((fd = XCC_UTIL_TEMP_FAILURE_RETRY(open(path, O_RDONLY | O_DIRECTORY | O_CLOEXEC))) < 0) goto end;
    
    while(1)
    {
        n = syscall(XC_UTIL_SYSCALL_GETDENTS, fd, buf, sizeof(buf));
        if(n <= 0) goto end;

        for(i = 0; i < n;)
        {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wcast-align"
            ent = (xc_util_dirent_t *)(buf + i);
#pragma clang diagnostic pop
            
            if(0 != memcmp(ent->d_name, ".", 1) &&
               0 != memcmp(ent->d_name, "..", 2) &&
               0 == xcc_util_atoi(ent->d_name, &tid))
                total++;
            
            i += ent->d_reclen;
        }
    }
    
 end:
    if(fd >= 0) close(fd);
    return total;
}

static size_t xc_fallback_get_process_thread(char *buf, size_t len, pid_t pid, pid_t tid)
{
    char  pname_buf[256];
    char  tname_buf[64];
    char *pname = "<unknown>";
    char *tname = "<unknown>";
    
    if(0 == xcc_util_get_process_name(pid, pname_buf, sizeof(pname_buf))) pname = pname_buf;
    if(0 == xcc_util_get_thread_name(tid, tname_buf, sizeof(tname_buf)))  tname = tname_buf;

    return xcc_fmt_snprintf(buf, len, "pid: %d, tid: %d, name: %s  >>> %s <<<\n",
                            pid, tid, tname, pname);
}

static size_t xc_fallback_get_signal(char *buf, size_t len, siginfo_t *si, pid_t pid)
{
    //fault addr
    char addr_desc[64];
    if(xcc_util_signal_has_si_addr(si))
        xcc_fmt_snprintf(addr_desc, sizeof(addr_desc), "%p", si->si_addr);
    else
        xcc_fmt_snprintf(addr_desc, sizeof(addr_desc), "--------");

    //from
    char sender_desc[64] = "";
    if(xcc_util_signal_has_sender(si, pid))
        xcc_fmt_snprintf(sender_desc, sizeof(sender_desc), " from pid %d, uid %d", si->si_pid, si->si_uid);

    return xcc_fmt_snprintf(buf, len, "signal %d (%s), code %d (%s%s), fault addr %s\n",
                            si->si_signo, xcc_util_get_signame(si),
                            si->si_code, xcc_util_get_sigcodename(si), sender_desc, addr_desc);
}

static size_t xc_fallback_get_regs(char *buf, size_t len, ucontext_t *uc)
{
#if defined(__arm__)
    return xcc_fmt_snprintf(buf, len, 
                            "    r0  %08x  r1  %08x  r2  %08x  r3  %08x\n"
                            "    r4  %08x  r5  %08x  r6  %08x  r7  %08x\n"
                            "    r8  %08x  r9  %08x  r10 %08x  r11 %08x\n"
                            "    ip  %08x  sp  %08x  lr  %08x  pc  %08x\n\n",
                            uc->uc_mcontext.arm_r0,
                            uc->uc_mcontext.arm_r1,
                            uc->uc_mcontext.arm_r2,
                            uc->uc_mcontext.arm_r3,
                            uc->uc_mcontext.arm_r4,
                            uc->uc_mcontext.arm_r5,
                            uc->uc_mcontext.arm_r6,
                            uc->uc_mcontext.arm_r7,
                            uc->uc_mcontext.arm_r8,
                            uc->uc_mcontext.arm_r9,
                            uc->uc_mcontext.arm_r10,
                            uc->uc_mcontext.arm_fp,
                            uc->uc_mcontext.arm_ip,
                            uc->uc_mcontext.arm_sp,
                            uc->uc_mcontext.arm_lr,
                            uc->uc_mcontext.arm_pc);
#elif defined(__aarch64__)
    return xcc_fmt_snprintf(buf, len, 
                            "    x0  %016lx  x1  %016lx  x2  %016lx  x3  %016lx\n"
                            "    x4  %016lx  x5  %016lx  x6  %016lx  x7  %016lx\n"
                            "    x8  %016lx  x9  %016lx  x10 %016lx  x11 %016lx\n"
                            "    x12 %016lx  x13 %016lx  x14 %016lx  x15 %016lx\n"
                            "    x16 %016lx  x17 %016lx  x18 %016lx  x19 %016lx\n"
                            "    x20 %016lx  x21 %016lx  x22 %016lx  x23 %016lx\n"
                            "    x24 %016lx  x25 %016lx  x26 %016lx  x27 %016lx\n"
                            "    x28 %016lx  x29 %016lx\n"
                            "    sp  %016lx  lr  %016lx  pc  %016lx\n\n",
                            uc->uc_mcontext.regs[0],
                            uc->uc_mcontext.regs[1],
                            uc->uc_mcontext.regs[2],
                            uc->uc_mcontext.regs[3],
                            uc->uc_mcontext.regs[4],
                            uc->uc_mcontext.regs[5],
                            uc->uc_mcontext.regs[6],
                            uc->uc_mcontext.regs[7],
                            uc->uc_mcontext.regs[8],
                            uc->uc_mcontext.regs[9],
                            uc->uc_mcontext.regs[10],
                            uc->uc_mcontext.regs[11],
                            uc->uc_mcontext.regs[12],
                            uc->uc_mcontext.regs[13],
                            uc->uc_mcontext.regs[14],
                            uc->uc_mcontext.regs[15],
                            uc->uc_mcontext.regs[16],
                            uc->uc_mcontext.regs[17],
                            uc->uc_mcontext.regs[18],
                            uc->uc_mcontext.regs[19],
                            uc->uc_mcontext.regs[20],
                            uc->uc_mcontext.regs[21],
                            uc->uc_mcontext.regs[22],
                            uc->uc_mcontext.regs[23],
                            uc->uc_mcontext.regs[24],
                            uc->uc_mcontext.regs[25],
                            uc->uc_mcontext.regs[26],
                            uc->uc_mcontext.regs[27],
                            uc->uc_mcontext.regs[28],
                            uc->uc_mcontext.regs[29],
                            uc->uc_mcontext.sp,
                            uc->uc_mcontext.regs[30], //lr
                            uc->uc_mcontext.pc);
#elif defined(__i386__)
    return xcc_fmt_snprintf(buf, len, 
                            "    eax %08x  ebx %08x  ecx %08x  edx %08x\n"
                            "    edi %08x  esi %08x\n"
                            "    ebp %08x  esp %08x  eip %08x\n\n",
                            uc->uc_mcontext.gregs[REG_EAX],
                            uc->uc_mcontext.gregs[REG_EBX],
                            uc->uc_mcontext.gregs[REG_ECX],
                            uc->uc_mcontext.gregs[REG_EDX],
                            uc->uc_mcontext.gregs[REG_EDI],
                            uc->uc_mcontext.gregs[REG_ESI],
                            uc->uc_mcontext.gregs[REG_EBP],
                            uc->uc_mcontext.gregs[REG_ESP],
                            uc->uc_mcontext.gregs[REG_EIP]);
#elif defined(__x86_64__)
    return xcc_fmt_snprintf(buf, len, 
                            "    rax %016lx  rbx %016lx  rcx %016lx  rdx %016lx\n"
                            "    r8  %016lx  r9  %016lx  r10 %016lx  r11 %016lx\n"
                            "    r12 %016lx  r13 %016lx  r14 %016lx  r15 %016lx\n"
                            "    rdi %016lx  rsi %016lx\n"
                            "    rbp %016lx  rsp %016lx  rip %016lx\n\n",
                            uc->uc_mcontext.gregs[REG_RAX],
                            uc->uc_mcontext.gregs[REG_RBX],
                            uc->uc_mcontext.gregs[REG_RCX],
                            uc->uc_mcontext.gregs[REG_RDX],
                            uc->uc_mcontext.gregs[REG_R8],
                            uc->uc_mcontext.gregs[REG_R9],
                            uc->uc_mcontext.gregs[REG_R10],
                            uc->uc_mcontext.gregs[REG_R11],
                            uc->uc_mcontext.gregs[REG_R12],
                            uc->uc_mcontext.gregs[REG_R13],
                            uc->uc_mcontext.gregs[REG_R14],
                            uc->uc_mcontext.gregs[REG_R15],
                            uc->uc_mcontext.gregs[REG_RDI],
                            uc->uc_mcontext.gregs[REG_RSI],
                            uc->uc_mcontext.gregs[REG_RBP],
                            uc->uc_mcontext.gregs[REG_RSP],
                            uc->uc_mcontext.gregs[REG_RIP]);
#endif
}

static size_t xc_fallback_get_backtrace(char *buf, size_t len, ucontext_t *uc, const char *ignore_lib)
{
    size_t used = 0;

    used += xcc_fmt_snprintf(buf + used, len - used, "backtrace:\n");
    used += xcc_unwind_get(uc, ignore_lib, buf + used, len - used);
    if(used >= len - 1)
    {
        buf[len - 3] = '\n';
        buf[len - 2] = '\0';
        used = len - 2;
    }
    used += xcc_fmt_snprintf(buf + used, len - used, "\n");
    return used;
}

static int xc_fallback_record_logcat_buffer(int fd, const char *pid_str,
                                             const char *buffer, unsigned int lines, const char *priority)
{
    char lines_str[16];
    int  r;
    
    xcc_fmt_snprintf(lines_str, sizeof(lines_str), "%u", lines);

    if(0 != (r = xcc_util_write_format_safe(fd,
                                            "--------- tail end of log %s "
                                            "(/system/bin/logcat -b %s -d -v threadtime -t %u%s%s %s)\n",
                                            buffer, buffer, lines, pid_str ? " --pid " : "",
                                            pid_str ? pid_str : "", priority))) return r;
    
    int child = fork();
    if(child < 0)
    {
        return 0; //just ignore
    }
    else if(0 == child)
    {
        //child...
        
        alarm(3); //don't leave a zombie process
        
        if(dup2(fd, STDOUT_FILENO) < 0) exit(0);

        if(pid_str)
            execl("/system/bin/logcat", "logcat", "-b", buffer, "-d", "-v", "threadtime",
                  "-t", lines_str, "--pid", pid_str, priority, (char *)NULL);
        else
            execl("/system/bin/logcat", "logcat", "-b", buffer, "-d", "-v", "threadtime",
                  "-t", lines_str, priority, (char *)NULL);
        
        exit(0);
    }
    
    //parent...
    waitpid(child, NULL, 0);
    return 0;
}

static int xc_fallback_record_logcat(int fd, pid_t pid, int api_level, unsigned int logcat_system_lines,
                                     unsigned int logcat_events_lines, unsigned int logcat_main_lines)
{
    char  buf[16];
    char *pid_str = NULL;
    int   r;

    if(api_level >= 24)
    {
        xcc_fmt_snprintf(buf, sizeof(buf), "%d", pid);
        pid_str = buf;
    }
    
    if(0 != (r = xcc_util_write_str(fd, "logcat:\n"))) return r;

    if(0 != (r = xc_fallback_record_logcat_buffer(fd, pid_str, "main",   logcat_main_lines,   "*:D"))) return r;
    if(0 != (r = xc_fallback_record_logcat_buffer(fd, pid_str, "system", logcat_system_lines, "*:W"))) return r;
    if(0 != (r = xc_fallback_record_logcat_buffer(fd, pid_str, "events", logcat_events_lines, "*:I"))) return r;

    if(0 != (r = xcc_util_write_str(fd, "\n"))) return r;

    return 0;
}

static int xc_fallback_record_fds(int fd, pid_t pid)
{
    int               fd2 = -1;
    char              path[128];
    char              fd_path[512];
    char              buf[512];
    long              n, i;
    int               fd_num;
    size_t            total = 0;
    xc_util_dirent_t *ent;
    ssize_t           len;
    int               r = 0;

    if(0 != (r = xcc_util_write_str(fd, "open files:\n"))) return r;

    xcc_fmt_snprintf(path, sizeof(path), "/proc/%d/fd", pid);
    if((fd2 = XCC_UTIL_TEMP_FAILURE_RETRY(open(path, O_RDONLY | O_DIRECTORY | O_CLOEXEC))) < 0) goto end;
    
    while(1)
    {
        n = syscall(XC_UTIL_SYSCALL_GETDENTS, fd2, buf, sizeof(buf));
        if(n <= 0) break;

        for(i = 0; i < n;)
        {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wcast-align"
            ent = (xc_util_dirent_t *)(buf + i);
#pragma clang diagnostic pop

            //get the fd
            if('\0' == ent->d_name[0]) goto next;
            if(0 == memcmp(ent->d_name, ".", 1)) goto next;
            if(0 == memcmp(ent->d_name, "..", 2)) goto next;
            if(0 != xcc_util_atoi(ent->d_name, &fd_num)) goto next;
            if(fd_num < 0) goto next;

            //count
            total++;
            if(total > 1024) goto next;

            //read link of the path
            xcc_fmt_snprintf(path, sizeof(path), "/proc/%d/fd/%d", pid, fd_num);
            len = readlink(path, fd_path, sizeof(fd_path) - 1);
            if(len <= 0 || len > (ssize_t)(sizeof(fd_path) - 1))
                strncpy(path, "???", sizeof(path));
            else
                fd_path[len] = '\0';
            
            //dump
            if(0 != (r = xcc_util_write_format_safe(fd, "    fd %d: %s\n", fd_num, fd_path))) goto clean;
            
        next:
            i += ent->d_reclen;
        }
    }

 end:
    if(total > 1024)
        if(0 != (r = xcc_util_write_str(fd, "    ......\n"))) goto clean;
    if(0 != (r = xcc_util_write_format_safe(fd, "    (number of FDs: %zu)\n", total))) goto clean;
    r = xcc_util_write_str(fd, "\n");

 clean:
    if(fd2 >= 0) close(fd2);
    return r;
}

size_t xc_fallback_get_emergency(siginfo_t *si,
                                 ucontext_t *uc,
                                 pid_t pid,
                                 pid_t tid,
                                 long tz,
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
                                 const char *build_fingerprint,
                                 const char *revision,
                                 char *emergency,
                                 size_t emergency_len)
{
    time_t       start_sec  = (time_t)(start_time / 1000000);
    suseconds_t  start_usec = (time_t)(start_time % 1000000);
    struct tm    start_tm;
    time_t       crash_sec   = (time_t)(crash_time / 1000000);
    suseconds_t  crash_usec = (time_t)(crash_time % 1000000);
    struct tm    crash_tm;
    char        *buf        = emergency;
    size_t       len        = emergency_len;
    size_t       used       = 0;

    //convert times
    memset(&crash_tm, 0, sizeof(crash_tm));
    memset(&start_tm, 0, sizeof(start_tm));
    xca_util_time2tm(start_sec, tz, &start_tm);
    xca_util_time2tm(crash_sec, tz, &crash_tm);

    //dump
    used += xcc_fmt_snprintf(buf + used, len - used, XCC_UTIL_TOMB_HEAD);
    used += xcc_fmt_snprintf(buf + used, len - used, "Tombstone maker: '%s'\n", XCC_VERSION_STR);
    used += xcc_fmt_snprintf(buf + used, len - used, "Crash type: '%s'\n", XCC_UTIL_CRASH_TYPE);
    used += xcc_fmt_snprintf(buf + used, len - used, XC_FALLBACK_TIME_FORMAT, "Start time",
                             start_tm.tm_year + 1900, start_tm.tm_mon + 1, start_tm.tm_mday,
                             start_tm.tm_hour, start_tm.tm_min, start_tm.tm_sec, start_usec / 1000,
                             tz < 0 ? '-' : '+', labs(tz / 3600), labs(tz % 3600));
    used += xcc_fmt_snprintf(buf + used, len - used, XC_FALLBACK_TIME_FORMAT, "Crash time",
                             crash_tm.tm_year + 1900, crash_tm.tm_mon + 1, crash_tm.tm_mday,
                             crash_tm.tm_hour, crash_tm.tm_min, crash_tm.tm_sec, crash_usec / 1000,
                             tz < 0 ? '-' : '+', labs(tz / 3600), labs(tz % 3600));
    used += xcc_fmt_snprintf(buf + used, len - used, "App ID: '%s'\n", app_id);
    used += xcc_fmt_snprintf(buf + used, len - used, "App version: '%s'\n", app_version);
    used += xc_fallback_get_cpu(buf + used, len - used);
    used += xc_fallback_get_system_mem(buf + used, len - used);
    used += xcc_fmt_snprintf(buf + used, len - used, "Number of threads: '%zu'\n", xc_fallback_get_number_of_threads(pid));
    used += xcc_fmt_snprintf(buf + used, len - used, "Rooted: '%s'\n", xcc_util_is_root() ? "Yes" : "No");
    used += xcc_fmt_snprintf(buf + used, len - used, "API level: '%d'\n", api_level);
    used += xcc_fmt_snprintf(buf + used, len - used, "OS version: '%s'\n", os_version);
    used += xcc_fmt_snprintf(buf + used, len - used, "Kernel version: '%s'\n", kernel_version);
    used += xcc_fmt_snprintf(buf + used, len - used, "ABI list: '%s'\n", abi_list);
    used += xcc_fmt_snprintf(buf + used, len - used, "Manufacturer: '%s'\n", manufacturer);
    used += xcc_fmt_snprintf(buf + used, len - used, "Brand: '%s'\n", brand);
    used += xcc_fmt_snprintf(buf + used, len - used, "Model: '%s'\n", model);
    used += xcc_fmt_snprintf(buf + used, len - used, "Build fingerprint: '%s'\n", build_fingerprint);
    used += xcc_fmt_snprintf(buf + used, len - used, "Revision: '%s'\n", revision);
    used += xcc_fmt_snprintf(buf + used, len - used, "ABI: '%s'\n", XCC_UTIL_ABI_STRING);
    used += xc_fallback_get_process_thread(buf + used, len - used, pid, tid);
    used += xc_fallback_get_signal(buf + used, len - used, si, pid);
    used += xc_fallback_get_regs(buf + used, len - used, uc);
    used += xc_fallback_get_backtrace(buf + used, len - used, uc, XCC_UTIL_XCRASH_FILENAME);
    return used;
}

int xc_fallback_record(int log_fd,
                       char *emergency,
                       pid_t pid,
                       int api_level,
                       unsigned int logcat_system_lines,
                       unsigned int logcat_events_lines,
                       unsigned int logcat_main_lines)
{
    int r;

    if(log_fd < 0) return XCC_ERRNO_INVAL;
    
    if(0 != (r = xcc_util_write_str(log_fd, emergency))) return r;
    emergency[0] = '\0'; //If we wrote the emergency info successfully, we don't need to return it from callback again.
    
    if(0 != (r = xc_fallback_record_logcat(log_fd, pid, api_level, logcat_system_lines, logcat_events_lines, logcat_main_lines))) return r;
    if(0 != (r = xc_fallback_record_fds(log_fd, pid))) return r;
    
    return 0;
}

#pragma clang diagnostic pop
