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
#include <dirent.h>
#include <string.h>
#include <regex.h>
#include <ctype.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <linux/elf.h>
#include "queue.h"
#include "xcc_errno.h"
#include "xcc_util.h"
#include "xcc_b64.h"
#include "xcc_meminfo.h"
#include "xcd_log.h"
#include "xcd_process.h"
#include "xcd_thread.h"
#include "xcd_maps.h"
#include "xcd_regs.h"
#include "xcd_util.h"
#include "xcd_sys.h"

typedef struct xcd_thread_info
{
    xcd_thread_t t;
    TAILQ_ENTRY(xcd_thread_info,) link;
} xcd_thread_info_t;
typedef TAILQ_HEAD(xcd_thread_info_queue, xcd_thread_info,) xcd_thread_info_queue_t;

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpadded"
struct xcd_process
{
    pid_t                    pid;
    char                    *pname;
    pid_t                    crash_tid;
    ucontext_t              *uc;
    siginfo_t               *si;
    xcd_thread_info_queue_t  thds;
    size_t                   nthds;
    xcd_maps_t              *maps;
};
#pragma clang diagnostic pop

static int xcd_process_load_threads(xcd_process_t *self)
{
    char               buf[128];
    DIR               *dir;
    struct dirent     *ent;
    pid_t              tid;
    xcd_thread_info_t *thd;

    snprintf(buf, sizeof(buf), "/proc/%d/task", self->pid);
    if(NULL == (dir = opendir(buf))) return XCC_ERRNO_SYS;
    while(NULL != (ent = readdir(dir)))
    {
        if(0 == strcmp(ent->d_name, ".")) continue;
        if(0 == strcmp(ent->d_name, "..")) continue;
        if(0 != xcc_util_atoi(ent->d_name, &tid)) continue;
        
        if(NULL == (thd = malloc(sizeof(xcd_thread_info_t)))) return XCC_ERRNO_NOMEM;
        xcd_thread_init(&(thd->t), self->pid, tid);
        
        TAILQ_INSERT_TAIL(&(self->thds), thd, link);
        self->nthds++;
    }
    closedir(dir);

    return 0;
}

int xcd_process_create(xcd_process_t **self, pid_t pid, pid_t crash_tid, siginfo_t *si, ucontext_t *uc)
{
    int                r;
    xcd_thread_info_t *thd;
    
    if(NULL == (*self = malloc(sizeof(xcd_process_t)))) return XCC_ERRNO_NOMEM;
    (*self)->pid       = pid;
    (*self)->pname     = NULL;
    (*self)->crash_tid = crash_tid;
    (*self)->si        = si;
    (*self)->uc        = uc;
    (*self)->nthds     = 0;
    TAILQ_INIT(&((*self)->thds));

    if(0 != (r = xcd_process_load_threads(*self)))
    {
        XCD_LOG_ERROR("PROCESS: load threads failed, errno=%d", r);
        return r;
    }

    //check if crashed thread existed
    TAILQ_FOREACH(thd, &((*self)->thds), link)
    {
        if(thd->t.tid == (*self)->crash_tid)
            return 0; //OK
    }

    XCD_LOG_ERROR("PROCESS: crashed thread NOT found");
    return XCC_ERRNO_NOTFND;
}

size_t xcd_process_get_number_of_threads(xcd_process_t *self)
{
    return self->nthds;
}

void xcd_process_suspend_threads(xcd_process_t *self)
{
    xcd_thread_info_t *thd;
    TAILQ_FOREACH(thd, &(self->thds), link)
        xcd_thread_suspend(&(thd->t));
}

void xcd_process_resume_threads(xcd_process_t *self)
{
    xcd_thread_info_t *thd;
    TAILQ_FOREACH(thd, &(self->thds), link)
        xcd_thread_resume(&(thd->t));
}

int xcd_process_load_info(xcd_process_t *self)
{
    int                r;
    xcd_thread_info_t *thd;
    char               buf[256];
    
    xcc_util_get_process_name(self->pid, buf, sizeof(buf));
    if(NULL == (self->pname = strdup(buf))) self->pname = "unknown";

    TAILQ_FOREACH(thd, &(self->thds), link)
    {
        //load thread info
        xcd_thread_load_info(&(thd->t));
        
        //load thread regs
        if(thd->t.tid != self->crash_tid)
            xcd_thread_load_regs(&(thd->t));
        else
            xcd_thread_load_regs_from_ucontext(&(thd->t), self->uc);
    }

    //load maps
    if(0 != (r = xcd_maps_create(&(self->maps), self->pid)))
        XCD_LOG_ERROR("PROCESS: create maps failed, errno=%d", r);

    return 0;
}

static int xcd_process_record_signal_info(xcd_process_t *self, int log_fd)
{
    //fault addr
    char addr_desc[64];
    if(xcc_util_signal_has_si_addr(self->si))
    {
        void *addr = self->si->si_addr;
        if(self->si->si_signo == SIGILL)
        {
            uint32_t instruction = 0;
            xcd_util_ptrace_read(self->pid, (uintptr_t)addr, &instruction, sizeof(instruction));
            snprintf(addr_desc, sizeof(addr_desc), "%p (*pc=%#08x)", addr, instruction);
        }
        else
        {
            snprintf(addr_desc, sizeof(addr_desc), "%p", addr);
        }
    }
    else
    {
        snprintf(addr_desc, sizeof(addr_desc), "--------");
    }

    //from
    char sender_desc[64] = "";
    if(xcc_util_signal_has_sender(self->si, self->pid))
    {
        snprintf(sender_desc, sizeof(sender_desc), " from pid %d, uid %d", self->si->si_pid, self->si->si_uid);
    }

    return xcc_util_write_format(log_fd, "signal %d (%s), code %d (%s%s), fault addr %s\n",
                                 self->si->si_signo, xcc_util_get_signame(self->si),
                                 self->si->si_code, xcc_util_get_sigcodename(self->si),
                                 sender_desc, addr_desc);
}

static int xcd_process_record_abort_message(xcd_process_t *self, int log_fd, int api_level)
{
    //
    // struct abort_msg_t {
    //     size_t size;
    //     char msg[0];
    // };
    //
    // abort_msg_t** __abort_message_ptr;
    //
    // ......
    // size_t size = sizeof(abort_msg_t) + strlen(msg) + 1;
    // ......
    //

    //get abort_msg_t ***ppp (&__abort_message_ptr)
    uintptr_t ppp = 0;
    if(api_level >= 29) ppp = xcd_maps_find_pc(self->maps, XCC_UTIL_LIBC_APEX, XCC_UTIL_LIBC_ABORT_MSG_PTR);
    if(0 == ppp) ppp = xcd_maps_find_pc(self->maps, XCC_UTIL_LIBC, XCC_UTIL_LIBC_ABORT_MSG_PTR);
    if(0 == ppp) return 0;
    XCD_LOG_DEBUG("PROCESS: abort_msg, ppp = %"PRIxPTR, ppp);

    //get abort_msg_t **pp (__abort_message_ptr)
    uintptr_t pp = 0;
    if(0 != xcd_util_ptrace_read_fully(self->pid, ppp, &pp, sizeof(uintptr_t))) return 0;
    if(0 == pp) return 0;
    XCD_LOG_DEBUG("PROCESS: abort_msg, pp = %"PRIxPTR, pp);

    //get abort_msg_t *p (*__abort_message_ptr)
    uintptr_t p = 0;
    if(0 != xcd_util_ptrace_read_fully(self->pid, pp, &p, sizeof(uintptr_t))) return 0;
    if(0 == p) return 0;
    XCD_LOG_DEBUG("PROCESS: abort_msg, p = %"PRIxPTR, p);

    //get p->size
    size_t size = 0;
    if(0 != xcd_util_ptrace_read_fully(self->pid, p, &size, sizeof(size_t))) return 0;
    if(size < sizeof(size_t) + 1 + 1) return 0;
    XCD_LOG_DEBUG("PROCESS: abort_msg, size = %zu", size);

    //get strlen(msg)
    size -= (sizeof(size_t) + 1);

    //get p->msg
    if(size > 256) size = 256;
    char msg[256 + 1];
    memset(msg, 0, sizeof(msg));
    if(0 != xcd_util_ptrace_read_fully(self->pid, p + sizeof(size_t), msg, size)) return 0;

    //format
    size_t i;
    for(i = 0; i < sizeof(msg); i++)
    {
        if(isspace(msg[i]) && ' ' != msg[i])
            msg[i] = ' ';
    }
    XCD_LOG_DEBUG("PROCESS: abort_msg, strlen(msg) = %zu", strlen(msg));

    //write
    return xcc_util_write_format(log_fd, "Abort message: '%s'\n", msg);
}

static regex_t *xcd_process_build_whitelist_regex(char *dump_all_threads_whitelist, size_t *re_cnt)
{
    if(NULL == dump_all_threads_whitelist || 0 == strlen(dump_all_threads_whitelist)) return NULL;

    char *p = dump_all_threads_whitelist;
    size_t cnt = 0;
    while(*p)
    {
        if(*p == '|') cnt++;
        p++;
    }
    cnt += 1;

    regex_t *re = NULL;
    if(NULL == (re = calloc(cnt, sizeof(regex_t)))) return NULL;

    char *tmp;
    char *regex_str = strtok_r(dump_all_threads_whitelist, "|", &tmp);
    char *regex_str_decoded;
    size_t i = 0;
    while(regex_str)
    {
        regex_str_decoded = (char *)xcc_b64_decode(regex_str, strlen(regex_str), NULL);
        if(0 == regcomp(&(re[i]), regex_str_decoded, REG_EXTENDED | REG_NOSUB))
        {
            XCD_LOG_DEBUG("PROCESS: compile regex OK: %s", regex_str_decoded);
            i++;
        }
        regex_str = strtok_r(NULL, "|", &tmp);
    }

    if(0 == i)
    {
        free(re);
        return NULL;
    }

    XCD_LOG_DEBUG("PROCESS: got %zu regex", i);
    *re_cnt = i;
    return re;
}

static int xcd_process_if_need_dump(char *tname, regex_t *re, size_t re_cnt)
{
    if(NULL == re || 0 == re_cnt) return 1;

    size_t i;
    for(i = 0; i < re_cnt; i++)
        if(0 == regexec(&(re[i]), tname, 0, NULL, 0)) return 1;

    return 0;
}

int xcd_process_record(xcd_process_t *self,
                       int log_fd,
                       unsigned int logcat_system_lines,
                       unsigned int logcat_events_lines,
                       unsigned int logcat_main_lines,
                       int dump_elf_hash,
                       int dump_map,
                       int dump_fds,
                       int dump_all_threads,
                       unsigned int dump_all_threads_count_max,
                       char *dump_all_threads_whitelist,
                       int api_level)
{
    int                r = 0;
    xcd_thread_info_t *thd;
    regex_t           *re = NULL;
    size_t             re_cnt = 0;
    unsigned int       thd_dumped = 0;
    int                thd_matched_regex = 0;
    int                thd_ignored_by_limit = 0;
    
    TAILQ_FOREACH(thd, &(self->thds), link)
    {
        if(thd->t.tid == self->crash_tid)
        {
            if(0 != (r = xcd_thread_record_info(&(thd->t), log_fd, self->pname))) return r;
            if(0 != (r = xcd_process_record_signal_info(self, log_fd))) return r;
            if(0 != (r = xcd_process_record_abort_message(self, log_fd, api_level))) return r;
            if(0 != (r = xcd_thread_record_regs(&(thd->t), log_fd))) return r;
            if(0 == xcd_thread_load_frames(&(thd->t), self->maps))
            {
                if(0 != (r = xcd_thread_record_backtrace(&(thd->t), log_fd))) return r;
                if(0 != (r = xcd_thread_record_buildid(&(thd->t), log_fd, dump_elf_hash, xcc_util_signal_has_si_addr(self->si) ? (uintptr_t)self->si->si_addr : 0))) return r;
                if(0 != (r = xcd_thread_record_stack(&(thd->t), log_fd))) return r;
                if(0 != (r = xcd_thread_record_memory(&(thd->t), log_fd))) return r;
            }
            if(dump_map) if(0 != (r = xcd_maps_record(self->maps, log_fd))) return r;
            if(0 != (r = xcc_util_record_logcat(log_fd, self->pid, api_level, logcat_system_lines, logcat_events_lines, logcat_main_lines))) return r;
            if(dump_fds) if(0 != (r = xcc_util_record_fds(log_fd, self->pid))) return r;
            if(0 != (r = xcc_meminfo_record(log_fd, self->pid))) return r;

            break;
        }
    }
    if(!dump_all_threads) return 0;

    //parse thread name whitelist regex
    re = xcd_process_build_whitelist_regex(dump_all_threads_whitelist, &re_cnt);

    TAILQ_FOREACH(thd, &(self->thds), link)
    {
        if(thd->t.tid != self->crash_tid)
        {
            //check regex for thread name
            if(NULL != re && re_cnt > 0 && !xcd_process_if_need_dump(thd->t.tname, re, re_cnt))
            {
                continue;
            }
            thd_matched_regex++;

            //check dump count limit
            if(dump_all_threads_count_max > 0 && thd_dumped >= dump_all_threads_count_max)
            {
                thd_ignored_by_limit++;
                continue;
            }

            if(0 != (r = xcc_util_write_str(log_fd, XCC_UTIL_THREAD_SEP))) goto end;
            if(0 != (r = xcd_thread_record_info(&(thd->t), log_fd, self->pname))) goto end;
            if(0 != (r = xcd_thread_record_regs(&(thd->t), log_fd))) goto end;
            if(0 == xcd_thread_load_frames(&(thd->t), self->maps))
            {
                if(0 != (r = xcd_thread_record_backtrace(&(thd->t), log_fd))) goto end;
                if(0 != (r = xcd_thread_record_stack(&(thd->t), log_fd))) goto end;
            }
            thd_dumped++;
        }
    }

 end:
    if(self->nthds > 1)
    {
        if(0 == thd_dumped)
            if(0 != (r = xcc_util_write_str(log_fd, XCC_UTIL_THREAD_SEP))) goto ret;

        if(0 != (r = xcc_util_write_format(log_fd, "total threads (exclude the crashed thread): %zu\n", self->nthds - 1))) goto ret;
        if(NULL != re && re_cnt > 0)
            if(0 != (r = xcc_util_write_format(log_fd, "threads matched whitelist: %d\n", thd_matched_regex))) goto ret;
        if(dump_all_threads_count_max > 0)
            if(0 != (r = xcc_util_write_format(log_fd, "threads ignored by max count limit: %d\n", thd_ignored_by_limit))) goto ret;
        if(0 != (r = xcc_util_write_format(log_fd, "dumped threads: %u\n", thd_dumped))) goto ret;
        
        if(0 != (r = xcc_util_write_str(log_fd, XCC_UTIL_THREAD_END))) goto ret;
    }
    
 ret:
    return r;
}
