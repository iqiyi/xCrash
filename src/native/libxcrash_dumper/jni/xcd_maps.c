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
#include "queue.h"
#include "xcc_errno.h"
#include "xcc_util.h"
#include "xcd_maps.h"
#include "xcd_map.h"
#include "xcd_util.h"
#include "xcd_log.h"
#include "xcd_recorder.h"

typedef struct xcd_maps_item
{
    xcd_map_t map;
    TAILQ_ENTRY(xcd_maps_item,) link;
} xcd_maps_item_t;
typedef TAILQ_HEAD(xcd_maps_item_queue, xcd_maps_item,) xcd_maps_item_queue_t;

struct xcd_maps
{
    xcd_maps_item_queue_t maps;
};

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

xcd_map_t *xcd_maps_find(xcd_maps_t *self, uintptr_t pc)
{
    xcd_maps_item_t *mi;

    TAILQ_FOREACH(mi, &(self->maps), link)
        if(pc >= mi->map.start && pc < mi->map.end)
            return &(mi->map);

    return NULL;
}

int xcd_maps_record(xcd_maps_t *self, xcd_recorder_t *recorder)
{
    xcd_maps_item_t *mi;
    int              r;

    if(0 != (r = xcd_recorder_write(recorder, "memory map:\n"))) return r;
                     
    TAILQ_FOREACH(mi, &(self->maps), link)
        if(0 != (r = xcd_map_record(&(mi->map), recorder))) return r;

    if(0 != (r = xcd_recorder_write(recorder, "\n"))) return r;

    return 0;
}
