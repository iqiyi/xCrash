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
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <android/log.h>
#include "xcc_errno.h"
#include "xcc_util.h"
#include "xcc_fmt.h"
#include "xc_recorder.h"
#include "xc_util.h"

// log filename format:
// prefix_01234567890123456789_version__pname[suffix]

#define XC_RECORDER_LOG_TS_LEN 20

struct xc_recorder
{
    char    *log_dir;
    char    *log_pathname;
    int      create_log_ok;
    
    char   **obsolete_logs;
    size_t   obsolete_logs_size;
    size_t   obsolete_logs_capacity;
};

static int xc_recorder_log_name_cmp(const void *a, const void *b)
{
    return strcmp(*((const char **)a), *((const char **)b));
}

static int xc_recorder_save_obsolete_logs(xc_recorder_t *self, const char *log_prefix, const char *log_suffix, size_t log_cnt_max)
{
    DIR            *dir = NULL;
    struct dirent  *entry;
    size_t          prefix_len = strlen(log_prefix);
    size_t          suffix_len = (log_suffix ? strlen(log_suffix) : 0);
    size_t          len;
    size_t          i;
    char          **logs_new;
    size_t          logs_capacity_new;
    int             r = 0;

    //unlimited logs
    if(0 == log_cnt_max) return 0;

    //load all logs
    if(NULL == (dir = opendir(self->log_dir))) return XCC_ERRNO_SYS;
    while(NULL != (entry = readdir(dir)))
    {
        //check file type
        if(DT_REG != entry->d_type) continue;

        //check file name
        len = strlen(entry->d_name);
        if(len < (prefix_len + 1 + XC_RECORDER_LOG_TS_LEN + 1 + suffix_len)) continue;
        if(0 != memcmp(entry->d_name, log_prefix, prefix_len)) continue;
        if('_' != entry->d_name[prefix_len]) continue;
        for(i = 0; i < XC_RECORDER_LOG_TS_LEN; i++)
            if(entry->d_name[prefix_len + 1 + i] < '0' || entry->d_name[prefix_len + 1 + i] > '9') continue;
        if(suffix_len > 0)
            if(0 != memcmp(entry->d_name + (len - suffix_len), log_suffix, suffix_len)) continue;

        if(self->obsolete_logs_size >= self->obsolete_logs_capacity)
        {
            //expand the buffer
            logs_capacity_new = self->obsolete_logs_size + 1;
            if(0 != logs_capacity_new % 10) logs_capacity_new += (10 - logs_capacity_new % 10);

            if(NULL == (logs_new = realloc(self->obsolete_logs, sizeof(char *) * logs_capacity_new)))
            {
                r = XCC_ERRNO_NOMEM;
                goto ret;
            }
            
            self->obsolete_logs = logs_new;
            self->obsolete_logs_capacity = logs_capacity_new;
        }

        //save new logs
        if(NULL == (self->obsolete_logs[self->obsolete_logs_size] = strdup(entry->d_name))) 
        {
            r = XCC_ERRNO_NOMEM;
            goto ret;
        }
        self->obsolete_logs_size++;
    }
    closedir(dir);
    dir = NULL;

    //no need to delete any log files
    if(self->obsolete_logs_size < log_cnt_max) goto ret;

    //sort
    qsort(self->obsolete_logs, self->obsolete_logs_size, sizeof(char *), xc_recorder_log_name_cmp);

    //Save obsolete log file names that need to be removed.
    //Do NOT remove them now. Because deleting is essentially a write to the file system
    //which may slow the APP startup process.
    for(i = self->obsolete_logs_size - (log_cnt_max - 1); i < self->obsolete_logs_size; i++)
        free(self->obsolete_logs[i]);
    self->obsolete_logs_size -= (log_cnt_max - 1);
    self->obsolete_logs_capacity = self->obsolete_logs_size;
    self->obsolete_logs = realloc(self->obsolete_logs, sizeof(char *) * self->obsolete_logs_capacity);
    
    return 0;

 ret:
    if(dir) closedir(dir);
    for(i = 0; i < self->obsolete_logs_size; i++)
        free(self->obsolete_logs[i]);
    self->obsolete_logs_size = 0;
    self->obsolete_logs_capacity = 0;
    if(self->obsolete_logs)
    {
        free(self->obsolete_logs);
        self->obsolete_logs = NULL;
    }
    return r;
}

int xcd_recorder_create(xc_recorder_t **self, uint64_t start_time, const char *app_version,
                        const char *log_dir, const char *log_prefix, const char *log_suffix,
                        size_t log_cnt_max)
{
    const char *prefix = log_prefix ? log_prefix : "tombstone";
    const char *suffix = log_suffix ? log_suffix : "";
    char        buf[PATH_MAX];
    char        process_name[512] = "\0";
    int         r = 0;
    
    if(NULL == (*self = malloc(sizeof(xc_recorder_t)))) return XCC_ERRNO_NOMEM;
    if(NULL == ((*self)->log_dir = strdup(log_dir)))
    {
        r =  XCC_ERRNO_NOMEM;
        goto err;
    }
    (*self)->log_pathname           = NULL;
    (*self)->create_log_ok          = 0;
    (*self)->obsolete_logs          = NULL;
    (*self)->obsolete_logs_size     = 0;
    (*self)->obsolete_logs_capacity = 0;

    //create log dirs
    if(0 != (r = xc_util_mkdirs(log_dir))) goto err;

    //get process name
    if(0 != xcc_util_get_process_name(getpid(), process_name, sizeof(process_name)))
        strncpy(process_name, "unknown", sizeof(process_name));
    
    //save current log pathname
    snprintf(buf, sizeof(buf), "%s/%s_%020"PRIu64"_%s__%s%s",
             log_dir, prefix, start_time, app_version, process_name, suffix);
    if(NULL == ((*self)->log_pathname = strdup(buf)))
    {
        r =  XCC_ERRNO_NOMEM;
        goto err;
    }
    
    //save obsolete log file names
    xc_recorder_save_obsolete_logs(*self, prefix, suffix, log_cnt_max);
    
    return 0;

 err:
    if(NULL != *self)
    {
        if(NULL != (*self)->log_dir) free((*self)->log_dir);
        free(*self);
        *self = NULL;        
    }
    return r;
}

static void xc_recorder_remove_obsolete_logs(xc_recorder_t *self)
{
    char   pathname[PATH_MAX];
    size_t i;
    
    if(NULL == self->obsolete_logs) return;

    for(i = 0; i < self->obsolete_logs_size; i++)
    {
        xcc_fmt_snprintf(pathname, sizeof(pathname), "%s/%s", self->log_dir, self->obsolete_logs[i]);
        unlink(pathname);
    }

    self->obsolete_logs = NULL;
}

int xc_recorder_create_log(xc_recorder_t *self)
{
    int  fd;

    //remove obsolete logs
    xc_recorder_remove_obsolete_logs(self);

    //create new log
    if(NULL == self->log_pathname) return XCC_ERRNO_INVAL;
    if((fd = open(self->log_pathname, O_CREAT | O_WRONLY | O_CLOEXEC | O_TRUNC, 0644)) < 0) return XCC_ERRNO_SYS;
    close(fd);

    self->create_log_ok = 1;
    return 0;
}

int xc_recorder_create_log_ok(xc_recorder_t *self)
{
    return self->create_log_ok;
}

char *xc_recorder_get_log_pathname(xc_recorder_t *self)
{
    return self->log_pathname;
}

int xc_recorder_log_err(xc_recorder_t *self, const char *msg, int errnum)
{
    int fd;
    int r;
    
    if(NULL == self->log_pathname || !self->create_log_ok) return XCC_ERRNO_STATE;

    if((fd = open(self->log_pathname, O_WRONLY | O_CLOEXEC | O_APPEND, 0644)) < 0) return XCC_ERRNO_SYS;
    r = xcc_util_write_format(fd, "\nxcrash error:\n%s, errnum=%d\n\n", msg, errnum);
    close(fd);
    
    return r;
}

int xc_recorder_log_err_msg(xc_recorder_t *self, const char *msg, int errnum, const char *errmsg)
{
    int fd;
    int r;
    
    if(NULL == self->log_pathname || !self->create_log_ok) return XCC_ERRNO_STATE;

    if((fd = open(self->log_pathname, O_WRONLY | O_CLOEXEC | O_APPEND, 0644)) < 0) return XCC_ERRNO_SYS;
    r = xcc_util_write_format(fd, "\nxcrash error:\n%s, errnum=%d, errmsg=%s\n\n", msg, errnum, errmsg);
    close(fd);
    
    return r;
}

int xc_recorder_open(xc_recorder_t *self, int *fd)
{
    if(NULL == self->log_pathname || !self->create_log_ok) return XCC_ERRNO_INVAL;
    if((*fd = open(self->log_pathname, O_WRONLY | O_CLOEXEC | O_APPEND, 0644)) < 0) return XCC_ERRNO_SYS;
    return 0;
}

void xc_recorder_close(xc_recorder_t *self, int fd)
{
    (void)self;
    
    if(fd >= 0) close(fd);
}

int xc_recorder_check_backtrace_valid(xc_recorder_t *self)
{
    int    fd = -1;
    char   line[256];
    size_t i = 0;
    int    r = 0;
    
    if(NULL == self->log_pathname || !self->create_log_ok) return 0;

    if((fd = TEMP_FAILURE_RETRY(open(self->log_pathname, O_RDONLY | O_CLOEXEC))) < 0) return 0;
    
    while(NULL != xcc_util_gets(line, sizeof(line), fd))
    {
        if(0 == memcmp(line, "backtrace:\n", 11))
        {
            //check the next line
            if(NULL != xcc_util_gets(line, sizeof(line), fd) && 0 == memcmp(line, "    #00 pc ", 11))
                r = 1; //we found the backtrace
            break;
        }
        if(i++ > 200) //check the top 200 lines at most
            break;
    }

    if(fd >= 0) close(fd);
    return r;    
}
