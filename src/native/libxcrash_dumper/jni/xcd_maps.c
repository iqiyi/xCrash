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
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/mman.h>
#include "queue.h"
#include "xcc_errno.h"
#include "xcc_util.h"
#include "xcd_maps.h"
#include "xcd_map.h"
#include "xcd_util.h"
#include "xcd_log.h"

typedef struct xcd_maps_item
{
    xcd_map_t map;
    TAILQ_ENTRY(xcd_maps_item,) link;
} xcd_maps_item_t;
typedef TAILQ_HEAD(xcd_maps_item_queue, xcd_maps_item,) xcd_maps_item_queue_t;

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpadded"
struct xcd_maps
{
    xcd_maps_item_queue_t maps;
    pid_t                 pid;
};
#pragma clang diagnostic pop

static int xcd_maps_parse_line(char *line, xcd_maps_item_t **mi)
{
    uintptr_t  start;
    uintptr_t  end;
    char       flags[5];
    size_t     offset;
    int        pos;
    char      *name;

    *mi = NULL;
    
    //scan
    if(sscanf(line, "%"SCNxPTR"-%"SCNxPTR" %4s %"SCNxPTR" %*x:%*x %*d%n", &start, &end, flags, &offset, &pos) != 4) return 0;
    name = xcc_util_trim(line + pos);
    
    //create map
    if(NULL == (*mi = malloc(sizeof(xcd_maps_item_t)))) return XCC_ERRNO_NOMEM;
    return xcd_map_init(&((*mi)->map), start, end, offset, flags, name);
}

int xcd_maps_create(xcd_maps_t **self, pid_t pid)
{
    char             buf[512];
    FILE            *fp;
    xcd_maps_item_t *mi;
    int              r;

    if(NULL == (*self = malloc(sizeof(xcd_maps_t)))) return XCC_ERRNO_NOMEM;
    TAILQ_INIT(&((*self)->maps));
    (*self)->pid = pid;

    snprintf(buf, sizeof(buf), "/proc/%d/maps", pid);
    if(NULL == (fp = fopen(buf, "r"))) return XCC_ERRNO_SYS;

    while(fgets(buf, sizeof(buf), fp))
    {
        if(0 != (r = xcd_maps_parse_line(buf, &mi)))
        {
            fclose(fp);
            return r;
        }
        
        if(NULL != mi)
            TAILQ_INSERT_TAIL(&((*self)->maps), mi, link);
    }
    
    fclose(fp);
    return 0;
}

void xcd_maps_destroy(xcd_maps_t **self)
{
    xcd_maps_item_t *mi, *mi_tmp;
    TAILQ_FOREACH_SAFE(mi, &((*self)->maps), link, mi_tmp)
    {
        TAILQ_REMOVE(&((*self)->maps), mi, link);
        xcd_map_uninit(&(mi->map));
        free(mi);
    }

    *self = NULL;
}

xcd_map_t *xcd_maps_find_map(xcd_maps_t *self, uintptr_t pc)
{
    xcd_maps_item_t *mi;

    TAILQ_FOREACH(mi, &(self->maps), link)
        if(pc >= mi->map.start && pc < mi->map.end)
            return &(mi->map);

    return NULL;
}

xcd_map_t *xcd_maps_get_prev_map(xcd_maps_t *self, xcd_map_t *cur_map)
{
    (void)self;

    xcd_maps_item_t *cur_mi = (xcd_maps_item_t *)cur_map;
    xcd_maps_item_t *prev_mi = TAILQ_PREV(cur_mi, xcd_maps_item_queue, link);

    return (NULL == prev_mi ? NULL : &(prev_mi->map));
}

uintptr_t xcd_maps_find_pc(xcd_maps_t *self, const char *pathname, const char *symbol)
{
    xcd_maps_item_t *mi;
    xcd_elf_t       *elf;
    uintptr_t        addr = 0;

    TAILQ_FOREACH(mi, &(self->maps), link)
    {
        if(NULL != mi->map.name && 0 == strcmp(mi->map.name, pathname))
        {
            //get ELF
            if(NULL == (elf = xcd_map_get_elf(&(mi->map), self->pid, (void *)self))) return 0;

            //get rel addr (offset)
            if(0 != xcd_elf_get_symbol_addr(elf, symbol, &addr)) return 0;

            return xcd_map_get_abs_pc(&(mi->map), addr, self->pid, (void *)self);
        }
    }

    return 0; //not found
}

int xcd_maps_record(xcd_maps_t *self, int log_fd)
{
    int              r;
    xcd_maps_item_t *mi;
    uintptr_t        size;
    uintptr_t        total_size = 0;
    size_t           max_size = 0;
    size_t           max_offset = 0;
    size_t           width_size = 0;
    size_t           width_offset = 0;
    uintptr_t        load_bias = 0;
    char             load_bias_buf[64] = "\0";
    char            *name = "";
    char            *prev_name = NULL;

    //get width of size and offset columns
    TAILQ_FOREACH(mi, &(self->maps), link)
    {
        size = mi->map.end - mi->map.start;
        if(size > max_size) max_size = size;
        if(mi->map.offset > max_offset) max_offset = mi->map.offset;
    }
    while(0 != max_size)
    {
        max_size /= 0x10;
        width_size++;
    }
    if(0 == width_size) width_size = 1;
    while(0 != max_offset)
    {
        max_offset /= 0x10;
        width_offset++;
    }
    if(0 == width_offset) width_offset = 1;

    //dump
    if(0 != (r = xcc_util_write_str(log_fd, "memory map:\n"))) return r;
    TAILQ_FOREACH(mi, &(self->maps), link)
    {
        //get load_bias
        if(NULL != mi->map.elf)
        {
            if(0 != (load_bias = xcd_elf_get_load_bias(mi->map.elf)))
                snprintf(load_bias_buf, sizeof(load_bias_buf), " (load base 0x%"PRIxPTR")", load_bias);
        }
        else
        {
            load_bias_buf[0] = '\0';
        }

        //get name
        if(NULL != mi->map.name)
        {
            if(NULL == prev_name)
                name = mi->map.name;
            else if(0 == strcmp(prev_name, mi->map.name))
                name = ">"; //same as prev line
            else
                name = mi->map.name;
        }
        else
        {
            name = "";
        }

        //save prev name
        prev_name = mi->map.name;

        //update total size
        size = mi->map.end - mi->map.start;
        total_size += size;

        if(0 != (r = xcc_util_write_format(log_fd,
                                           "    %0"XCC_UTIL_FMT_ADDR"-%0"XCC_UTIL_FMT_ADDR" %c%c%c %*"PRIxPTR" %*"PRIxPTR" %s%s\n",
                                           mi->map.start, mi->map.end,
                                           mi->map.flags & PROT_READ ? 'r' : '-',
                                           mi->map.flags & PROT_WRITE ? 'w' : '-',
                                           mi->map.flags & PROT_EXEC ? 'x' : '-',
                                           width_offset, mi->map.offset,
                                           width_size, size,
                                           name, load_bias_buf))) return r;
    }
    if(0 != (r = xcc_util_write_format(log_fd, "    TOTAL SIZE: 0x%"PRIxPTR"K (%"PRIuPTR"K)\n\n",
                                       total_size / 1024, total_size / 1024))) return r;

    return 0;
}
