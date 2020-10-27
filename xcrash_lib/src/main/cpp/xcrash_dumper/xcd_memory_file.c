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

#include <unistd.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "xcc_errno.h"
#include "xcc_util.h"
#include "xcd_map.h"
#include "xcd_memory.h"
#include "xcd_memory_file.h"
#include "xcd_util.h"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpadded"
struct xcd_memory_file
{
    xcd_memory_t *base;
    int           fd;
    uint8_t      *data;
    size_t        offset;
    size_t        size;
};
#pragma clang diagnostic pop

static void xcd_memory_file_uninit(xcd_memory_file_t *self)
{
    if(NULL != self->data)
    {
        munmap(self->data - self->offset, self->size + self->offset);
        self->data = NULL;
        self->offset = 0;
        self->size = 0;
    }
}

static int xcd_memory_file_init(xcd_memory_file_t *self, size_t size, size_t offset, uint64_t file_size)
{
    xcd_memory_file_uninit(self);

    if(offset >= (size_t)file_size) return XCC_ERRNO_RANGE;

    size_t aligned_offset = offset & (~(size_t)(getpagesize() - 1));
    if(aligned_offset > (size_t)file_size) return XCC_ERRNO_RANGE;

    self->offset = offset & (size_t)(getpagesize() - 1);
    self->size = (size_t)file_size - aligned_offset;
    
    size_t max_size;
    if(!__builtin_add_overflow(size, self->offset, &max_size) && max_size < self->size)
        self->size = max_size;

    void* map = mmap(NULL, self->size, PROT_READ, MAP_PRIVATE, self->fd, (off_t)aligned_offset);
    if(map == MAP_FAILED) return XCC_ERRNO_SYS;

    self->data = (uint8_t *)map + self->offset;
    self->size -= self->offset;

    return 0;
}

int xcd_memory_file_create(void **obj, xcd_memory_t *base, xcd_map_t *map, xcd_maps_t *maps)
{
    xcd_memory_file_t **self = (xcd_memory_file_t **)obj;
    size_t              map_size = map->end - map->start;
    xcd_map_t          *prev_map;
    size_t              prev_map_size;
    struct stat         st;
    uint64_t            file_size;
    size_t              max_size;
    int                 r;

    if(NULL == map->name || 0 == strlen(map->name)) return XCC_ERRNO_INVAL;
    
    if(NULL == (*self = malloc(sizeof(xcd_memory_file_t)))) return XCC_ERRNO_NOMEM;
    (*self)->base = base;
    (*self)->fd = -1;
    (*self)->data = NULL;
    (*self)->offset = 0;
    (*self)->size = 0;

    //open file
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wgnu-statement-expression"
    if(0 > ((*self)->fd = XCC_UTIL_TEMP_FAILURE_RETRY(open(map->name, O_RDONLY | O_CLOEXEC))))
    {
        r = XCC_ERRNO_SYS;
        goto err;
    }
#pragma clang diagnostic pop

    //get file size
    if(0 != fstat((*self)->fd, &st))
    {
        r = XCC_ERRNO_SYS;
        goto err;
    }
    file_size = (uint64_t)st.st_size;

    //CASE 1: Offset is zero.
    //        The whole file is an ELF?
    //
    // -->    d9b9c000-d9bb6000 r-xp 00000000 fd:00 2666  /system/lib/libjavacrypto.so
    //        d9bb6000-d9bb7000 r--p 00019000 fd:00 2666  /system/lib/libjavacrypto.so
    //        d9bb7000-d9bb8000 rw-p 0001a000 fd:00 2666  /system/lib/libjavacrypto.so
    //
    if(0 == map->offset)
    {
        if(0 != (r = xcd_memory_file_init(*self, SIZE_MAX, 0, file_size))) goto err;
        if(!xcd_elf_is_valid(base))
        {
            r = XCC_ERRNO_MEM;
            goto err;
        }
        return 0;
    }

    //CASE 2: Offset is not zero.
    //        The start of this map is an ELF header? (ELF embedded in another file)
    //
    //        cc2aa000-ce2aa000 rw-p 00000000 00:01 3811616  /dev/ashmem/dalvik-data-code-cache (deleted)
    //        ce2aa000-d02aa000 r-xp 00000000 00:01 3811617  /dev/ashmem/dalvik-jit-code-cache (deleted)
    // -->    d02aa000-d286d000 r-xp 048b3000 fd:00 623      /system/app/WebViewGoogle/WebViewGoogle.apk
    //        d286d000-d286e000 ---p 00000000 00:00 0
    //
    if(0 != (r = xcd_memory_file_init(*self, map_size, map->offset, file_size))) goto err;
    if(xcd_elf_is_valid(base))
    {
        map->elf_start_offset = map->offset;
        
        max_size = xcd_elf_get_max_size(base);
        if(max_size > map_size)
        {
            //try to map the whole file
            if(0 != xcd_memory_file_init(*self, max_size, map->offset, file_size))
            {
                //rollback
                if(0 != (r = xcd_memory_file_init(*self, map_size, map->offset, file_size))) goto err;
            }
        }
        return 0;
    }

    //CASE 3: Offset is not zero.
    //        No ELF header at the start of this map.
    //        The whole file is an ELF? (this map is part of an ELF file)
    //
    //        72a12000-72a1e000 r--p 00000000 fd:00 1955  /system/framework/arm/boot-apache-xml.oat
    // -->    72a1e000-72a36000 r-xp 0000c000 fd:00 1955  /system/framework/arm/boot-apache-xml.oat
    //        72a36000-72a37000 r--p 00024000 fd:00 1955  /system/framework/arm/boot-apache-xml.oat
    //        72a37000-72a38000 rw-p 00025000 fd:00 1955  /system/framework/arm/boot-apache-xml.oat
    //
    if(0 != (r = xcd_memory_file_init(*self, SIZE_MAX, 0, file_size))) goto err;
    if(xcd_elf_is_valid(base))
    {
        map->elf_offset = map->offset;
        return 0;
    }

    //CASE 4: Offset is not zero.
    //        No ELF header at the start of this map.
    //        The whole file is not an ELF.
    //        The start of the previous map is an ELF header? (this map is part of an ELF which embedded in another file)
    //
    //        d1ea6000-d256d000 r--p 0095b000 fc:00 1158  /system/app/Chrome/Chrome.apk
    // -->    d256d000-d5ff0000 r-xp 01022000 fc:00 1158  /system/app/Chrome/Chrome.apk
    //        d5ff0000-d6009000 rw-p 04aa5000 fc:00 1158  /system/app/Chrome/Chrome.apk
    //
    prev_map = xcd_maps_get_prev_map(maps, map);
    if(NULL != prev_map && PROT_READ == prev_map->flags && map->offset > prev_map->offset &&
       NULL != prev_map->name && 0 == strcmp(prev_map->name, map->name))
    {
        prev_map_size = prev_map->end - prev_map->start;
        if(0 != (r = xcd_memory_file_init(*self, prev_map_size, prev_map->offset, file_size))) goto err;
        if(xcd_elf_is_valid(base))
        {
            max_size = xcd_elf_get_max_size(base);
            if(max_size > prev_map_size)
            {
                if(0 != (r = xcd_memory_file_init(*self, max_size, prev_map->offset, file_size))) goto err;
                map->elf_offset = map->offset - prev_map->offset;
                map->elf_start_offset = prev_map->offset;
                return 0;
            }
        }
    }

    //CASE 5: ELF not found.
    //
    // -->    eaa6d000-eaab9000 r--s 00000000 fd:00 1178 /system/fonts/Roboto-Thin.ttf
    // -->    eaabd000-eaabe000 rw-p 00000000 00:00 0    [anon:.bss]
    // -->    eaffc000-eb037000 r--s 00000000 07:08 18   /apex/com.android.tzdata/etc/icu/icu_tzdata.dat
    // -->    eb466000-eb486000 rw-p 00000000 00:00 0    [anon:dalvik-LinearAlloc]
    // -->    ......
    //
    r = XCC_ERRNO_NOTFND;
    
 err:
    map->elf_offset = 0;
    map->elf_start_offset = 0;
    xcd_memory_file_uninit(*self);
    if((*self)->fd < 0) close((*self)->fd);
    free(*self);
    *self = NULL;
    return r;
}

void xcd_memory_file_destroy(void **obj)
{
    xcd_memory_file_t **self = (xcd_memory_file_t **)obj;
    
    xcd_memory_file_uninit(*self);
    close((*self)->fd);
    free(*self);
    *self = NULL;
}

size_t xcd_memory_file_read(void *obj, uintptr_t addr, void *dst, size_t size)
{
    xcd_memory_file_t *self = (xcd_memory_file_t *)obj;
    
    if(addr >= self->size) return 0;

    size_t bytes_left = self->size - (size_t)addr;
    uint8_t *actual_base = self->data + addr;

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wgnu-statement-expression"
    size_t actual_len = XCC_UTIL_MIN(bytes_left, size);
#pragma clang diagnostic pop

    memcpy(dst, actual_base, actual_len);
    return actual_len;
}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-variable-declarations"
const xcd_memory_handlers_t xcd_memory_file_handlers = {
    xcd_memory_file_destroy,
    xcd_memory_file_read
};
#pragma clang diagnostic pop
