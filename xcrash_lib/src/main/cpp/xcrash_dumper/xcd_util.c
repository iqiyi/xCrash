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

#include <fcntl.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <ctype.h>
#include <string.h>
#include <signal.h>
#include <inttypes.h>
#include <sys/ptrace.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "xcc_errno.h"
#include "xcc_util.h"
#include "xcd_util.h"
#include "xcd_log.h"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wreserved-id-macro"
#pragma clang diagnostic ignored "-Wpadded"
#include "7zCrc.h"
#include "Xz.h"
#include "XzCrc64.h"
#pragma clang diagnostic pop

extern __attribute((weak)) ssize_t process_vm_readv(pid_t, const struct iovec *, unsigned long, const struct iovec *, unsigned long, unsigned long);

static size_t xcd_util_process_vm_readv(pid_t pid, uintptr_t remote_addr, void* dst, size_t dst_len)
{
    size_t page_size = (size_t)sysconf(_SC_PAGE_SIZE);
    struct iovec src_iovs[64];
    uintptr_t cur_remote_addr = remote_addr;
    size_t total_read = 0;

    while (dst_len > 0)
    {
        // the destination
        struct iovec dst_iov = {.iov_base = &((uint8_t *)dst)[total_read], .iov_len = dst_len};

        // the source
        size_t iovecs_used = 0;
        while (dst_len > 0)
        {
            // fill iov_base
            src_iovs[iovecs_used].iov_base = (void *)cur_remote_addr;

            // fill iov_len (one page at a time, page boundaries aligned)
            uintptr_t misalignment = cur_remote_addr & (page_size - 1);
            size_t iov_len = page_size - misalignment;
            src_iovs[iovecs_used].iov_len = (iov_len > dst_len ? dst_len : iov_len);

            if (__builtin_add_overflow(cur_remote_addr, iov_len, &cur_remote_addr)) return total_read;
            dst_len -= iov_len;

            if(64 == ++iovecs_used) break;
        }

        // read from source to destination
        ssize_t rc;
        if(NULL != process_vm_readv)
            rc = process_vm_readv(pid, &dst_iov, 1, src_iovs, iovecs_used, 0);
        else
            rc = syscall(__NR_process_vm_readv, pid, &dst_iov, 1, src_iovs, iovecs_used, 0);
        if(-1 == rc) return total_read;

        total_read += (size_t)rc;
    }

    return total_read;
}

static size_t xcd_util_original_ptrace(pid_t pid, uintptr_t remote_addr, void *dst, size_t dst_len)
{
    // Make sure that there is no overflow.
    uintptr_t max_size;
    if(__builtin_add_overflow(remote_addr, dst_len, &max_size)) return 0;

    size_t bytes_read = 0;
    long   data;
    size_t align_bytes = remote_addr & (sizeof(long) - 1);
    if(align_bytes != 0)
    {
        if(0 != xcd_util_ptrace_read_long(pid, remote_addr & ~(sizeof(long) - 1), &data)) return 0;

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wgnu-statement-expression"
        size_t copy_bytes = XCC_UTIL_MIN(sizeof(long) - align_bytes, dst_len);
#pragma clang diagnostic pop
        
        memcpy(dst, (uint8_t *)(&data) + align_bytes, copy_bytes);
        remote_addr += copy_bytes;
        dst = (void *)((uintptr_t)dst + copy_bytes);
        dst_len -= copy_bytes;
        bytes_read += copy_bytes;
    }

    size_t i;
    for(i = 0; i < dst_len / sizeof(long); i++)
    {
        if(0 != xcd_util_ptrace_read_long(pid, remote_addr, &data)) return bytes_read;
        
        memcpy(dst, &data, sizeof(long));
        dst = (void *)((uintptr_t)dst + sizeof(long));
        remote_addr += sizeof(long);
        bytes_read += sizeof(long);
    }

    size_t left_over = dst_len & (sizeof(long) - 1);
    if(left_over)
    {
        if(0 != xcd_util_ptrace_read_long(pid, remote_addr, &data)) return bytes_read;
        
        memcpy(dst, &data, left_over);
        bytes_read += left_over;
    }

    return bytes_read;
}

size_t xcd_util_ptrace_read(pid_t pid, uintptr_t remote_addr, void *dst, size_t dst_len)
{
    static size_t (*ptrace_read)(pid_t, uintptr_t, void *, size_t) = NULL;

    if(NULL != ptrace_read)
    {
        return ptrace_read(pid, remote_addr, dst, dst_len);
    }
    else
    {
        size_t bytes = xcd_util_process_vm_readv(pid, remote_addr, dst, dst_len);
        if(bytes > 0)
        {
            __atomic_store_n(&ptrace_read, xcd_util_process_vm_readv, __ATOMIC_SEQ_CST);
            return bytes;
        }
        bytes = xcd_util_original_ptrace(pid, remote_addr, dst, dst_len);
        if(bytes > 0)
        {
            __atomic_store_n(&ptrace_read, xcd_util_original_ptrace, __ATOMIC_SEQ_CST);
            return bytes;
        }
        return 0;
    }
}

int xcd_util_ptrace_read_fully(pid_t pid, uintptr_t addr, void *dst, size_t bytes)
{
    size_t rc = xcd_util_ptrace_read(pid, addr, dst, bytes);
    return rc == bytes ? 0 : XCC_ERRNO_MISSING;
}


int xcd_util_ptrace_read_long(pid_t pid, uintptr_t addr, long *value)
{
    // ptrace() returns -1 and sets errno when the operation fails.
    // To disambiguate -1 from a valid result, we clear errno beforehand.
    errno = 0;
    *value = ptrace(PTRACE_PEEKTEXT, pid, (void *)addr, NULL);
    if(-1 == *value && 0 != errno)
    {
        //XCD_LOG_INFO("UTIL: ptrace error, addr:%"PRIxPTR", errno:%d\n", addr, errno);
        return errno;
    }

    return 0;
}

static void* xcd_util_xz_alloc(ISzAllocPtr p, size_t size)
{
    (void)p;
    return malloc(size);
}

static void xcd_util_xz_free(ISzAllocPtr p, void* address)
{
    (void)p;
    free(address);
}

static int xcd_util_xz_crc_gen = 0;
int xcd_util_xz_decompress(uint8_t* src, size_t src_size, uint8_t** dst, size_t* dst_size)
{
    size_t       src_offset = 0;
    size_t       dst_offset = 0;
    size_t       src_remaining;
    size_t       dst_remaining;
    ISzAlloc     alloc = {.Alloc = xcd_util_xz_alloc, .Free = xcd_util_xz_free};
    CXzUnpacker  state;
    ECoderStatus status;

    if(!xcd_util_xz_crc_gen)
    {
        //call these initialization functions only once
        xcd_util_xz_crc_gen = 1;
        
        CrcGenerateTable();
        Crc64GenerateTable();
    }

    XzUnpacker_Construct(&state, &alloc);
    
    *dst_size = 2 * src_size;
    *dst = NULL;
    do
    {
        *dst_size *= 2;
        if(NULL == (*dst = realloc(*dst, *dst_size)))
        {
            XzUnpacker_Free(&state);
            return XCC_ERRNO_NOMEM;
        }
        
        src_remaining = src_size - src_offset;
        dst_remaining = *dst_size - dst_offset;
        
        if(SZ_OK != XzUnpacker_Code(&state, *dst + dst_offset, &dst_remaining,
                                    src + src_offset, &src_remaining, 1, CODER_FINISH_ANY, &status))
        {
            free(*dst);
            XzUnpacker_Free(&state);
            return XCC_ERRNO_FORMAT;
        }
        src_offset += src_remaining;
        dst_offset += dst_remaining;
    } while (status == CODER_STATUS_NOT_FINISHED);
    
    XzUnpacker_Free(&state);
    
    if(!XzUnpacker_IsStreamWasFinished(&state))
    {
        free(*dst);
        return XCC_ERRNO_FORMAT;
    }
    
    *dst_size = dst_offset;
    *dst = realloc(*dst, *dst_size);
    
    return 0;
}
