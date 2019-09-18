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
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <linux/elf.h>
#include "xcc_errno.h"
#include "xcc_util.h"
#include "xcd_thread.h"
#include "xcd_frames.h"
#include "xcd_regs.h"
#include "xcd_util.h"
#include "xcd_log.h"

void xcd_thread_init(xcd_thread_t *self, pid_t pid, pid_t tid)
{
    self->status = XCD_THREAD_STATUS_OK;
    self->pid    = pid;
    self->tid    = tid;
    self->tname  = NULL;
    self->frames = NULL;
    memset(&(self->regs), 0, sizeof(self->regs));
}

void xcd_thread_suspend(xcd_thread_t *self)
{
    if(0 != ptrace(PTRACE_ATTACH, self->tid, NULL, NULL))
    {
#if XCD_THREAD_DEBUG
        XCD_LOG_WARN("THREAD: ptrace ATTACH failed, errno=%d", errno);
#endif
        self->status = XCD_THREAD_STATUS_ATTACH;
        return;
    }

    errno = 0;
    while(waitpid(self->tid, NULL, __WALL) < 0)
    {
        if(EINTR != errno)
        {
            ptrace(PTRACE_DETACH, self->tid, NULL, NULL);
#if XCD_THREAD_DEBUG
            XCD_LOG_ERROR("THREAD: waitpid for ptrace ATTACH failed, errno=%d", errno);
#endif
            self->status = XCD_THREAD_STATUS_ATTACH_WAIT;
            return;
        }
        errno = 0;
    }
}

void xcd_thread_resume(xcd_thread_t *self)
{
    ptrace(PTRACE_DETACH, self->tid, NULL, NULL);
}

void xcd_thread_load_info(xcd_thread_t *self)
{
    char buf[64] = "\0";
    
    xcc_util_get_thread_name(self->tid, buf, sizeof(buf));
    if(NULL == (self->tname = strdup(buf))) self->tname = "unknown";
}

void xcd_thread_load_regs(xcd_thread_t *self)
{
    uintptr_t regs[64]; //big enough for all architectures
    size_t    regs_len;
    
#ifdef PTRACE_GETREGS
    if(0 != ptrace(PTRACE_GETREGS, self->tid, NULL, &regs))
    {
        XCD_LOG_ERROR("THREAD: ptrace GETREGS failed, errno=%d", errno);
        self->status = XCD_THREAD_STATUS_REGS;
        return;
    }
    regs_len = XCD_REGS_USER_NUM;
#else
    struct iovec iovec;
    iovec.iov_base = &regs;
    iovec.iov_len = sizeof(regs);
    if(0 != ptrace(PTRACE_GETREGSET, self->tid, (void *)NT_PRSTATUS, &iovec))
    {
        XCD_LOG_ERROR("THREAD: ptrace GETREGSET failed, errno=%d", errno);
        self->status = XCD_THREAD_STATUS_REGS;
        return;
    }
    regs_len = iovec.iov_len / sizeof(uintptr_t);
#endif
    xcd_regs_load_from_ptregs(&(self->regs), regs, regs_len);
}

void xcd_thread_load_regs_from_ucontext(xcd_thread_t *self, ucontext_t *uc)
{
    xcd_regs_load_from_ucontext(&(self->regs), uc);
}

int xcd_thread_load_frames(xcd_thread_t *self, xcd_maps_t *maps)
{
#if XCD_THREAD_DEBUG
    XCD_LOG_DEBUG("THREAD: load frames, tid=%d, tname=%s", self->tid, self->tname);
#endif

    if(XCD_THREAD_STATUS_OK != self->status) return XCC_ERRNO_STATE; //do NOT ignore

    return xcd_frames_create(&(self->frames), &(self->regs), maps, self->pid);
}

int xcd_thread_record_info(xcd_thread_t *self, int log_fd, const char *pname)
{
    return xcc_util_write_format(log_fd, "pid: %d, tid: %d, name: %s  >>> %s <<<\n",
                                 self->pid, self->tid, self->tname, pname);
}

int xcd_thread_record_regs(xcd_thread_t *self, int log_fd)
{
    if(XCD_THREAD_STATUS_OK != self->status) return 0; //ignore
    
    return xcd_regs_record(&(self->regs), log_fd);
}

int xcd_thread_record_backtrace(xcd_thread_t *self, int log_fd)
{
    if(XCD_THREAD_STATUS_OK != self->status) return 0; //ignore

    return xcd_frames_record_backtrace(self->frames, log_fd);
}

int xcd_thread_record_buildid(xcd_thread_t *self, int log_fd, int dump_elf_hash, uintptr_t fault_addr)
{
    if(XCD_THREAD_STATUS_OK != self->status) return 0; //ignore

    return xcd_frames_record_buildid(self->frames, log_fd, dump_elf_hash, fault_addr);
}

int xcd_thread_record_stack(xcd_thread_t *self, int log_fd)
{
    if(XCD_THREAD_STATUS_OK != self->status) return 0; //ignore
    
    return xcd_frames_record_stack(self->frames, log_fd);
}

#define XCD_THREAD_MEMORY_BYTES_TO_DUMP 256
#define XCD_THREAD_MEMORY_BYTES_PER_LINE 16

static int xcd_thread_record_memory_by_addr(xcd_thread_t *self, int log_fd,
                                            const char *label, uintptr_t addr)
{
    int r;
    
    // Align the address to sizeof(long) and start 32 bytes before the address.
    addr &= ~(sizeof(long) - 1);
    if (addr >= 4128) addr -= 32;

    // Don't bother if the address looks too low, or looks too high.
    if (addr < 4096 ||
#if defined(__LP64__)
        addr > 0x4000000000000000UL - XCD_THREAD_MEMORY_BYTES_TO_DUMP) {
#else
        addr > 0xffff0000 - XCD_THREAD_MEMORY_BYTES_TO_DUMP) {
#endif
        return 0; //not an error
    }

    if(0 != (r = xcc_util_write_format(log_fd, "memory near %s:\n", label))) return r;
    
    // Dump 256 bytes
    uintptr_t data[XCD_THREAD_MEMORY_BYTES_TO_DUMP/sizeof(uintptr_t)];
    memset(data, 0, XCD_THREAD_MEMORY_BYTES_TO_DUMP);
    size_t bytes = xcd_util_ptrace_read(self->pid, addr, data, sizeof(data));
    if (bytes % sizeof(uintptr_t) != 0)
        bytes &= ~(sizeof(uintptr_t) - 1);
    
    uint64_t start = 0;
    int skip_2nd_read = 0;
    if(bytes == 0)
    {
        // In this case, we might want to try another read at the beginning of
        // the next page only if it's within the amount of memory we would have
        // read.
        size_t page_size = (size_t)sysconf(_SC_PAGE_SIZE);
        start = ((addr + (page_size - 1)) & ~(page_size - 1)) - addr;
        if (start == 0 || start >= XCD_THREAD_MEMORY_BYTES_TO_DUMP)
            skip_2nd_read = 1;
    }

    if (bytes < XCD_THREAD_MEMORY_BYTES_TO_DUMP && !skip_2nd_read)
    {
        // Try to do one more read. This could happen if a read crosses a map,
        // but the maps do not have any break between them. Or it could happen
        // if reading from an unreadable map, but the read would cross back
        // into a readable map. Only requires one extra read because a map has
        // to contain at least one page, and the total number of bytes to dump
        // is smaller than a page.
        size_t bytes2 = xcd_util_ptrace_read(self->pid, (uintptr_t)(addr + start + bytes), (uint8_t *)(data) + bytes,
                                             (size_t)(sizeof(data) - bytes - start));
        bytes += bytes2;
        if(bytes2 > 0 && bytes % sizeof(uintptr_t) != 0)
            bytes &= ~(sizeof(uintptr_t) - 1);
    }
    uintptr_t *data_ptr = data;
    uint8_t *ptr;
    size_t current = 0;
    size_t total_bytes = (size_t)(start + bytes);
    size_t i, j, k;
    char ascii[XCD_THREAD_MEMORY_BYTES_PER_LINE + 1];
    size_t ascii_idx = 0;
    char line[128];
    size_t line_len = 0;
    for(j = 0; j < XCD_THREAD_MEMORY_BYTES_TO_DUMP / XCD_THREAD_MEMORY_BYTES_PER_LINE; j++)
    {
        ascii_idx = 0;
        
        line_len = (size_t)snprintf(line, sizeof(line), "    %0"XCC_UTIL_FMT_ADDR, addr);
        addr += XCD_THREAD_MEMORY_BYTES_PER_LINE;

        for(i = 0; i < XCD_THREAD_MEMORY_BYTES_PER_LINE / sizeof(uintptr_t); i++)
        {
            if(current >= start && current + sizeof(uintptr_t) <= total_bytes)
            {
                line_len += (size_t)snprintf(line + line_len, sizeof(line) - line_len, " %0"XCC_UTIL_FMT_ADDR, *data_ptr);
                
                // Fill out the ascii string from the data.
                ptr = (uint8_t *)data_ptr;
                for(k = 0; k < sizeof(uintptr_t); k++, ptr++)
                {
                    if(*ptr >= 0x20 && *ptr < 0x7f)
                    {
                        ascii[ascii_idx++] = (char)(*ptr);
                    }
                    else
                    {
                        ascii[ascii_idx++] = '.';
                    }
                }
                data_ptr++;
            }
            else
            {
                line_len += (size_t)snprintf(line + line_len, sizeof(line) - line_len, " ");
                for(k = 0; k < sizeof(uintptr_t) * 2; k++)
                    line_len += (size_t)snprintf(line + line_len, sizeof(line) - line_len, "-");
                for(k = 0; k < sizeof(uintptr_t); k++)
                    ascii[ascii_idx++] = '.';
            }
            current += sizeof(uintptr_t);
        }
        ascii[ascii_idx] = '\0';

        if(0 != (r = xcc_util_write_format(log_fd, "%s  %s\n", line, ascii))) return r;
    }

    if(0 != (r = xcc_util_write_str(log_fd, "\n"))) return r;

    return 0;
}

int xcd_thread_record_memory(xcd_thread_t *self, int log_fd)
{
    xcd_regs_label_t *labels;
    size_t            labels_count;
    size_t            i;
    int               r;

    if(XCD_THREAD_STATUS_OK != self->status) return 0; //ignore

    xcd_regs_get_labels(&labels, &labels_count);
    
    for(i = 0; i < labels_count; i++)
        if(0 != (r = xcd_thread_record_memory_by_addr(self, log_fd, labels[i].name,
                                                      (uintptr_t)(self->regs.r[labels[i].idx])))) return r;

    return 0;
}
