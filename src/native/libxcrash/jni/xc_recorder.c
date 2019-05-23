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
    int      if_create_new_file;
    int      prepared_fd;
};

int xcd_recorder_create(xc_recorder_t **self, uint64_t start_time, const char *app_version,
                        const char *log_dir, const char *log_prefix, const char *log_suffix,
                        char **log_pathname)
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
    (*self)->log_pathname       = NULL;
    (*self)->if_create_new_file = 0;
    (*self)->prepared_fd        = -1;
    
    //create log dirs
    if(0 != (r = xc_util_mkdirs(log_dir))) goto err;

    //get process name
    if(0 != xcc_util_get_process_name(getpid(), process_name, sizeof(process_name)))
        strncpy(process_name, "unknown", sizeof(process_name));
    
    //save and return current log pathname
    snprintf(buf, sizeof(buf), "%s/%s_%020"PRIu64"_%s__%s%s",
             log_dir, prefix, start_time, app_version, process_name, suffix);
    if(NULL == ((*self)->log_pathname = strdup(buf)))
    {
        r =  XCC_ERRNO_NOMEM;
        goto err;
    }
    *log_pathname = (*self)->log_pathname;

    //the prepared fd for fd exhaust
    (*self)->prepared_fd = TEMP_FAILURE_RETRY(open("/dev/null", O_RDWR));

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

int xc_recorder_create_and_open(xc_recorder_t *self)
{
    int               fd = -1;
    char              buf[512];
    char              pathname[PATH_MAX];
    int               n, i;
    xc_util_dirent_t *ent;
    int               dir_flags = O_RDONLY | O_DIRECTORY | O_CLOEXEC;
    int               file_flags = O_RDWR | O_CLOEXEC;
    int               new_file_flags = O_CREAT | O_WRONLY | O_CLOEXEC | O_TRUNC | O_APPEND;

    if(NULL == self->log_pathname) return -1;

    //open dir
    if((fd = TEMP_FAILURE_RETRY(open(self->log_dir, dir_flags))) < 0)
    {
        if(self->prepared_fd >= 0)
        {
            close(self->prepared_fd);
            self->prepared_fd = -1;
            if((fd = TEMP_FAILURE_RETRY(open(self->log_dir, dir_flags))) < 0) goto new_file;
        }
    }

    //try to rename a placeholder file and open it
    while((n = syscall(XC_UTIL_SYSCALL_GETDENTS, fd, buf, sizeof(buf))) > 0)
    {
        for(i = 0; i < n; i += ent->d_reclen)
        {
            ent = (xc_util_dirent_t *)(buf + i);
            
            // placeholder_12345678901234567890.clean.xcrash
            // file name length: 45
            if(45 == strlen(ent->d_name) &&
               0 == memcmp(ent->d_name, "placeholder_", 12) &&
               0 == memcmp(ent->d_name + 32, ".clean.xcrash", 13))
            {
                xcc_fmt_snprintf(pathname, sizeof(pathname), "%s/%s", self->log_dir, ent->d_name);
                if(0 == rename(pathname, self->log_pathname))
                {
                    close(fd);
                    fd = -1;
                    return TEMP_FAILURE_RETRY(open(self->log_pathname, file_flags));
                }
            }
        }
    }
    if(fd >= 0)
    {
        close(fd);
        fd = -1;
    }
    
 new_file:
    //create new file
    self->if_create_new_file = 1;
    
    if((fd = TEMP_FAILURE_RETRY(open(self->log_pathname, new_file_flags, 0644))) >= 0) return fd;
    
    if(self->prepared_fd >= 0)
    {
        close(self->prepared_fd);
        self->prepared_fd = -1;
        if((fd = TEMP_FAILURE_RETRY(open(self->log_pathname, new_file_flags, 0644))) >= 0) return fd;
    }

    return -1;
}

int xc_recorder_seek_to_end(xc_recorder_t *self, int log_fd)
{
    uint8_t buf[1024];
    ssize_t readed, n;
    off_t   offset = 0;

    //new file
    if(self->if_create_new_file) return log_fd;
    
    //placeholder file
    if(lseek(log_fd, 0, SEEK_SET) < 0) goto err;
    while(1)
    {
        readed = TEMP_FAILURE_RETRY(read(log_fd, buf, sizeof(buf)));
        if(readed < 0)
        {
            goto err;
        }
        else if(0 == readed)
        {
            if(lseek(log_fd, 0, SEEK_END) < 0) goto err;
            return log_fd;
        }
        else
        {
            for(n = readed; n > 0; n--)
            {
                if(0 != buf[n - 1]) break;
            }
            offset += (off_t)n;
            if(n < readed)
            {
                if(lseek(log_fd, offset, SEEK_SET) < 0) goto err;
                return log_fd;
            }
        }
    }

 err:
    close(log_fd);
    return -1;
}

int xc_recorder_check_backtrace_valid(xc_recorder_t *self)
{
    int    fd = -1;
    char   line[512];
    size_t i = 0;
    int    r = 0;
    
    if(NULL == self->log_pathname) return 0;

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
