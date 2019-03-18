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

struct xcd_memory_file
{
    xcd_memory_t *base;
    int           fd;
    uint8_t      *data;
    size_t        offset;
    size_t        size;
};

static int xcd_memory_file_init(xcd_memory_file_t *self, size_t size, size_t offset, uint64_t file_size)
{
    if(offset >= (size_t)file_size) return XCC_ERRNO_RANGE;

    size_t aligned_offset = offset & ~(getpagesize() - 1);
    if(aligned_offset > (size_t)file_size) return XCC_ERRNO_RANGE;

    self->offset = offset & (getpagesize() - 1);
    self->size = (size_t)file_size - aligned_offset;
    
    size_t max_size;
    if(!__builtin_add_overflow(size, self->offset, &max_size) && max_size < self->size)
        self->size = max_size;

    void* map = mmap(NULL, self->size, PROT_READ, MAP_PRIVATE, self->fd, aligned_offset);
    if(map == MAP_FAILED) return XCC_ERRNO_SYS;

    self->data = (uint8_t *)map + self->offset;
    self->size -= self->offset;

    return 0;
}

static void xcd_memory_file_uninit(xcd_memory_file_t *self)
{
    munmap(self->data - self->offset, self->size + self->offset);
    self->data = NULL;
    self->offset = 0;
    self->size = 0;
}

int xcd_memory_file_create(void **obj, xcd_memory_t *base, void *map_obj)
{

    xcd_memory_file_t **self = (xcd_memory_file_t **)obj;
    xcd_map_t          *map = (xcd_map_t *)map_obj;
    size_t              map_size = map->end - map->start;
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
    if(0 > ((*self)->fd = TEMP_FAILURE_RETRY(open(map->name, O_RDONLY | O_CLOEXEC))))
    {
        r = XCC_ERRNO_SYS;
        goto err;
    }

    //get file size
    if(0 != fstat((*self)->fd, &st))
    {
        r = XCC_ERRNO_SYS;
        goto err;
    }
    file_size = (uint64_t)st.st_size;

    if(0 == map->offset)
    {
        if(0 != (r = xcd_memory_file_init(*self, SIZE_MAX, 0, file_size))) goto err;
        if(!xcd_elf_is_valid(base))
        {
            r = XCC_ERRNO_MEM;
            goto err;
        }
        return 0; //(1) the whole file is an elf (elf_file_offset_in_current_map == 0)
    }

    if(0 != (r = xcd_memory_file_init(*self, map_size, map->offset, file_size))) goto err;
    if(!xcd_elf_is_valid(base))
    {
        //the whole file is an elf?
        xcd_memory_file_uninit(*self);
        if(0 != (r = xcd_memory_file_init(*self, SIZE_MAX, 0, file_size))) goto err;
        if(!xcd_elf_is_valid(base))
        {
            r = XCC_ERRNO_MEM;
            goto err;
        }
        
        map->elf_offset = map->offset; //save the elf file offset in the current map info
        return 0; //(2) the whole file is an elf (elf_file_offset_in_current_map != 0)
    }
    else
    {
        //elf file embedded in another file
        max_size = xcd_elf_get_max_size(base);
        if(max_size > map_size)
        {
            //try to map the whole file
            xcd_memory_file_uninit(*self);
            if(0 != xcd_memory_file_init(*self, max_size, map->offset, file_size))
            {
                //rollback
                if(0 != (r = xcd_memory_file_init(*self, map_size, map->offset, file_size))) goto err;
            }
        }
            
        return 0; //(3) elf file embedded in another file (elf_file_offset_in_current_map == 0)
    }
    
 err:
    if(NULL != (*self)->data) xcd_memory_file_uninit(*self);
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
    size_t actual_len = XCC_UTIL_MIN(bytes_left, size);

    memcpy(dst, actual_base, actual_len);
    return actual_len;
}

const xcd_memory_handlers_t xcd_memory_file_handlers = {
    xcd_memory_file_destroy,
    xcd_memory_file_read
};
