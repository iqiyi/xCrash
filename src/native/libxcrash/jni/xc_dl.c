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

// Created by caikelun on 2019-08-16.

#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <link.h>
#include <elf.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "xcc_errno.h"
#include "xcc_util.h"
#include "xc_dl.h"
#include "queue.h"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpadded"
#pragma clang diagnostic ignored "-Wgnu-statement-expression"

typedef struct xc_dl_symbols
{
    size_t sym_offset;
    size_t sym_end;
    size_t sym_entry_size;
    size_t str_offset;
    size_t str_end;
    TAILQ_ENTRY(xc_dl_symbols,) link;
} xc_dl_symbols_t;
typedef TAILQ_HEAD(xc_dl_symbols_queue, xc_dl_symbols,) xc_dl_symbols_queue_t;

struct xc_dl
{
    uintptr_t              map_start;
    int                    fd;
    uint8_t               *data;
    size_t                 size;
    uintptr_t              load_bias;
    xc_dl_symbols_queue_t  symbolsq;
};

static int xc_dl_find_map_start(xc_dl_t *self, const char *pathname)
{
    FILE      *f = NULL;
    char       line[512];
    uintptr_t  offset;
    int        pos;
    char      *p;
    int        r = XCC_ERRNO_NOTFND;

    if(NULL == (f = fopen("/proc/self/maps", "r"))) return XCC_ERRNO_SYS;
    while(fgets(line, sizeof(line), f))
    {
        if(2 != sscanf(line, "%"SCNxPTR"-%*"SCNxPTR" %*4s %"SCNxPTR" %*x:%*x %*d%n", &(self->map_start), &offset, &pos)) continue;
        if(0 != offset) continue;
        p = xcc_util_trim(line + pos);
        if(0 != strcmp(p, pathname)) continue;

        r = 0; //found
        break;
    }

    fclose(f);
    return r;
}

static int xc_dl_file_open(xc_dl_t *self, const char *pathname)
{
    struct stat st;

    //open file
    if(0 > (self->fd = XCC_UTIL_TEMP_FAILURE_RETRY(open(pathname, O_RDONLY | O_CLOEXEC)))) return XCC_ERRNO_SYS;

    //get file size
    if(0 != fstat(self->fd, &st) || 0 == st.st_size) return XCC_ERRNO_SYS;
    self->size = (size_t)st.st_size;

    //mmap the file
    if(MAP_FAILED == (self->data = (uint8_t *)mmap(NULL, self->size, PROT_READ, MAP_PRIVATE, self->fd, 0))) return XCC_ERRNO_SYS;
    
    return 0;
}

static void *xc_dl_file_get(xc_dl_t *self, uintptr_t offset, size_t size)
{
    if(offset + size > self->size) return NULL;
    return (void *)(self->data + offset);
}

static char *xc_dl_file_get_string(xc_dl_t *self, uintptr_t offset)
{
    uint8_t *p = self->data + offset;
    
    while(p < self->data + self->size)
    {
        if('\0' == *p) return (char *)(self->data + offset);
        p++;
    }
    
    return NULL;
}

static int xc_dl_parse_elf(xc_dl_t *self)
{
    ElfW(Ehdr)      *ehdr;
    ElfW(Phdr)      *phdr;
    ElfW(Shdr)      *shdr, *str_shdr;
    xc_dl_symbols_t *symbols;
    size_t           i, cnt = 0;
    
    //get ELF header
    if(NULL == (ehdr = xc_dl_file_get(self, 0, sizeof(ElfW(Ehdr))))) return XCC_ERRNO_FORMAT;

    //find load_bias in program headers
    for(i = 0; i < ehdr->e_phnum * ehdr->e_phentsize; i += ehdr->e_phentsize)
    {
        if(NULL == (phdr = xc_dl_file_get(self, ehdr->e_phoff + i, sizeof(ElfW(Phdr))))) return XCC_ERRNO_FORMAT;

        if((PT_LOAD == phdr->p_type) && (phdr->p_flags & PF_X) && (0 == phdr->p_offset))
        {
            self->load_bias = phdr->p_vaddr;
            break;
        }
    }

    //find symbol tables in section headers
    for(i = ehdr->e_shentsize; i < ehdr->e_shnum * ehdr->e_shentsize; i += ehdr->e_shentsize)
    {
        if(NULL == (shdr = xc_dl_file_get(self, ehdr->e_shoff + i, sizeof(ElfW(Shdr))))) return XCC_ERRNO_FORMAT;

        if(SHT_SYMTAB == shdr->sh_type || SHT_DYNSYM == shdr->sh_type)
        {
            if(shdr->sh_link >= ehdr->e_shnum) continue;
            if(NULL == (str_shdr = xc_dl_file_get(self, ehdr->e_shoff + shdr->sh_link * ehdr->e_shentsize, sizeof(ElfW(Shdr))))) return XCC_ERRNO_FORMAT;
            if(SHT_STRTAB != str_shdr->sh_type) continue;
            
            if(NULL == (symbols = malloc(sizeof(xc_dl_symbols_t)))) return XCC_ERRNO_NOMEM;
            symbols->sym_offset = shdr->sh_offset;
            symbols->sym_end = shdr->sh_offset + shdr->sh_size;
            symbols->sym_entry_size = shdr->sh_entsize;
            symbols->str_offset = str_shdr->sh_offset;
            symbols->str_end = str_shdr->sh_offset + str_shdr->sh_size;
            TAILQ_INSERT_TAIL(&(self->symbolsq), symbols, link);
            cnt++;
        }
    }
    if(0 == cnt) return XCC_ERRNO_FORMAT;

    return 0;
}

xc_dl_t *xc_dl_create(const char *pathname)
{
    xc_dl_t *self;

    if(NULL == (self = calloc(1, sizeof(xc_dl_t)))) return NULL;
    self->fd = -1;
    self->data = MAP_FAILED;
    TAILQ_INIT(&(self->symbolsq));

    if(0 != xc_dl_find_map_start(self, pathname)) goto err;
    if(0 != xc_dl_file_open(self, pathname)) goto err;
    if(0 != xc_dl_parse_elf(self)) goto err;
    
    return self;

 err:
    xc_dl_destroy(&self);
    return NULL;
}

void xc_dl_destroy(xc_dl_t **self)
{
    xc_dl_symbols_t *symbols, *symbols_tmp;
    
    if(NULL == self || NULL == *self) return;
    
    if(MAP_FAILED != (*self)->data) munmap((*self)->data, (*self)->size);
    
    if((*self)->fd >= 0) close((*self)->fd);
    
    TAILQ_FOREACH_SAFE(symbols, &((*self)->symbolsq), link, symbols_tmp)
    {
        TAILQ_REMOVE(&((*self)->symbolsq), symbols, link);
        free(symbols);
    }
    
    free(*self);
    *self = NULL;
}

void *xc_dl_sym(xc_dl_t *self, const char *symbol)
{
    xc_dl_symbols_t *symbols;
    ElfW(Sym)       *sym;
    size_t           offset, str_offset;
    char            *str;

    TAILQ_FOREACH(symbols, &(self->symbolsq), link)
    {
        for(offset = symbols->sym_offset; offset < symbols->sym_end; offset += symbols->sym_entry_size)
        {
            //read .symtab / .dynsym
            if(NULL == (sym = xc_dl_file_get(self, offset, sizeof(ElfW(Sym))))) break;
            if(SHN_UNDEF == sym->st_shndx) continue;

            //read .strtab / .dynstr
            str_offset = symbols->str_offset + sym->st_name;
            if(str_offset >= symbols->str_end) continue;
            if(NULL == (str = xc_dl_file_get_string(self, str_offset))) continue;

            //compare symbol name
            if(0 != strcmp(symbol, str)) continue;

            //found
            return (void *)(self->map_start + sym->st_value - self->load_bias);
        }
    }
    
    return NULL;
}

#pragma clang diagnostic pop
