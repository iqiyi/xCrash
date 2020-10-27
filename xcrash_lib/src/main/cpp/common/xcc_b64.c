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

// Created by caikelun on 2019-03-12.

#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/types.h>
#include "xcc_b64.h"

static const char xcc_b64_table[65] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

size_t xcc_b64_encode_max_len(size_t in_len)
{
    return in_len * 4 / 3 + 4 + 1;
}

char *xcc_b64_encode(const uint8_t *in, size_t in_len, size_t *out_len)
{
    const uint8_t *start, *end;
    char *out, *p;
    size_t olen;

    olen = in_len * 4 / 3 + 4 + 1;
    if(olen < in_len) return NULL;
    if(NULL == (out = malloc(olen))) return NULL;
    
    start = in;
    end = in + in_len;
    p = out;
    while (end - start >= 3)
    {
        *p++ = xcc_b64_table[start[0] >> 2];
        *p++ = xcc_b64_table[((start[0] & 0x03) << 4) | (start[1] >> 4)];
        *p++ = xcc_b64_table[((start[1] & 0x0f) << 2) | (start[2] >> 6)];
        *p++ = xcc_b64_table[start[2] & 0x3f];
        start += 3;
    }

    if (end - start > 0)
    {
        *p++ = xcc_b64_table[start[0] >> 2];
        if(end - start == 1)
        {
            *p++ = xcc_b64_table[(start[0] & 0x03) << 4];
            *p++ = '=';
        }
        else //end - start == 2
        {
            *p++ = xcc_b64_table[((start[0] & 0x03) << 4) | (start[1] >> 4)];
            *p++ = xcc_b64_table[(start[1] & 0x0f) << 2];
        }
        *p++ = '=';
    }

    *p = '\0';
    
    if(NULL != out_len) *out_len = (size_t)(p - out);
    
    return out;
}

size_t xcc_b64_decode_max_len(size_t in_len)
{
    return in_len / 4 * 3 + 1;
}

uint8_t *xcc_b64_decode(const char *in, size_t in_len, size_t *out_len)
{
    uint8_t dtable[256], *out, *p, block[4], tmp;
    size_t i, count, olen, pad = 0;

    //build decode table
    memset(dtable, 0x80, 256);
    for(i = 0; i < sizeof(xcc_b64_table) - 1; i++)
        dtable[(size_t)(xcc_b64_table[i])] = (uint8_t)i;
    dtable['='] = 0;

    //get & check valid count of char at the "in" buffer
    count = 0;
    for (i = 0; i < in_len; i++) {
        if (dtable[(size_t)(in[i])] != 0x80)
            count++;
    }
    if(0 == count || 0 != count % 4) return NULL;

    //"out" buffer
    olen = count / 4 * 3 + 1;
    if(NULL == (out = malloc(olen))) return NULL;

    p = out;
    count = 0;
    for (i = 0; i < in_len; i++)
    {
        tmp = dtable[(size_t)(in[i])];
        if(0x80 == tmp) continue; //invalid char

        if('=' == in[i]) pad++; //meet padding
        
        block[count] = tmp;
        count++;
        if (count == 4)
        {
            *p++ = (uint8_t)((block[0] << 2) | (block[1] >> 4));
            *p++ = (uint8_t)((block[1] << 4) | (block[2] >> 2));
            *p++ = (uint8_t)((block[2] << 6) | block[3]);
            count = 0;
            if (pad)
            {
                if (pad == 1)
                    p--;
                else if (pad == 2)
                    p -= 2;
                else {
                    free(out);
                    return NULL;
                }
                break;
            }
        }
    }
    
    *p = 0;

    if(NULL != out_len) *out_len = (size_t)(p - out);
    return out;
}
