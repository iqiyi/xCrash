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

#include <stdint.h>
#include <sys/types.h>
#include <stdarg.h>
#include <string.h>
#include "xcc_fmt.h"
#include "xcc_libc_support.h"

static unsigned xcc_fmt_parse_decimal(const char *format, int *ppos)
{
    const char *p = format + *ppos;
    unsigned result = 0;
    for(;;)
    {
        int ch = *p;
        unsigned d = (unsigned)(ch - '0');
        if(d >= 10U)
        {
            break;
        }
        result = result * 10 + d;
        p++;
    }
    *ppos = (int)(p - format);
    return result;
}

static void xcc_fmt_format_unsigned(char *buf, size_t buf_size, uint64_t value, int base, int caps)
{
    char* p = buf;
    char* end = buf + buf_size - 1;
    
    // Generate digit string in reverse order.
    while (value)
    {
        unsigned d = (unsigned)(value % (uint64_t)base);
        value /= (uint64_t)base;
        if (p != end)
        {
            char ch;
            if (d < 10)
            {
                ch = '0' + (char)d;
            }
            else
            {
                ch = (caps ? 'A' : 'a') + (char)(d - 10);
            }
            *p++ = ch;
        }
    }
    
    // Special case for 0.
    if (p == buf)
    {
        if (p != end)
        {
            *p++ = '0';
        }
    }
    *p = '\0';
    
    // Reverse digit string in-place.
    size_t length = (size_t)(p - buf);
    for (size_t i = 0, j = length - 1; i < j; ++i, --j)
    {
        char ch = buf[i];
        buf[i] = buf[j];
        buf[j] = ch;
    }
}

static void xcc_fmt_format_integer(char* buf, size_t buf_size, uint64_t value, char conversion)
{
    // Decode the conversion specifier.
    int is_signed = (conversion == 'd' || conversion == 'i' || conversion == 'o');
    int base = 10;
    if (conversion == 'x' || conversion == 'X')
    {
        base = 16;
    }
    else if (conversion == 'o')
    {
        base = 8;
    }
    int caps = (conversion == 'X');
    if (is_signed && (int64_t)(value) < 0)
    {
        buf[0] = '-';
        buf += 1;
        buf_size -= 1;
        value = (uint64_t)(-(int64_t)(value));
    }
    xcc_fmt_format_unsigned(buf, buf_size, value, base, caps);
}

//format stream
typedef struct {
    size_t  total;
    char   *pos;
    size_t  avail;
} xcc_fmt_stream_t;

static void xcc_fmt_stream_init(xcc_fmt_stream_t *self, char *buffer, size_t buffer_size)
{
    self->total = 0;
    self->pos   = buffer;
    self->avail = buffer_size;

    if(self->avail > 0) self->pos[0] = '\0';
}

static size_t xcc_fmt_stream_total(xcc_fmt_stream_t *self)
{
    return self->total;
}

static void xcc_fmt_stream_send(xcc_fmt_stream_t *self, const char *data, int len)
{
    if(len < 0)
    {
        len = (int)strlen(data);
    }
    self->total += (size_t)len;
    
    if(self->avail <= 1)
    {
        //no space to put anything else
        return;
    }
    
    if((size_t)len >= self->avail)
    {
        len = (int)(self->avail - 1);
    }
    
    memcpy(self->pos, data, (size_t)len);
    self->pos += len;
    self->pos[0] = '\0';
    self->avail -= (size_t)len;
}

static void xcc_fmt_stream_send_repeat(xcc_fmt_stream_t *self, char ch, int count)
{
    char pad[8];
    xcc_libc_support_memset(pad, ch, sizeof(pad));
    
    const int pad_size = (int)(sizeof(pad));
    while(count > 0)
    {
        int avail = count;
        if (avail > pad_size)
        {
            avail = pad_size;
        }
        xcc_fmt_stream_send(self, pad, avail);
        count -= avail;
    }
}

static void xcc_fmt_stream_vformat(xcc_fmt_stream_t *self, const char *format, va_list args)
{
    int nn = 0;
    
    for(;;)
    {
        int    mm;
        int    padZero = 0;
        int    padLeft = 0;
        char   sign = '\0';
        int    width = -1;
        int    prec = -1;
        size_t bytelen = sizeof(int);
        int    slen;
        char   buffer[32]; //temporary buffer used to format numbers
        char   c;
        
        //first, find all characters that are not 0 or '%', then send them to the output directly
        mm = nn;
        do
        {
            c = format[mm];
            if(c == '\0' || c == '%') break;
            mm++;
        }while(1);
        if(mm > nn)
        {
            xcc_fmt_stream_send(self, format + nn, mm - nn);
            nn = mm;
        }
        
        //is this it ? then exit
        if (c == '\0') break;
        
        //nope, we are at a '%' modifier
        nn++;// skip it
        
        //parse flags
        for(;;)
        {
            c = format[nn++];
            if (c == '\0')
            {
                //single trailing '%' ?
                c = '%';
                xcc_fmt_stream_send(self, &c, 1);
                return;
            }
            else if(c == '0')
            {
                padZero = 1;
                continue;
            }
            else if (c == '-')
            {
                padLeft = 1;
                continue;
            }
            else if(c == ' ' || c == '+')
            {
                sign = c;
                continue;
            }
            break;
        }
        
        //parse field width
        if((c >= '0' && c <= '9'))
        {
            nn--;
            width = (int)(xcc_fmt_parse_decimal(format, &nn));
            c = format[nn++];
        }
        
        //parse precision
        if(c == '.')
        {
            prec = (int)(xcc_fmt_parse_decimal(format, &nn));
            c = format[nn++];
        }
        
        //length modifier
        switch(c)
        {
        case 'h':
            bytelen = sizeof(short);
            if(format[nn] == 'h')
            {
                bytelen = sizeof(char);
                nn += 1;
            }
            c = format[nn++];
            break;
        case 'l':
            bytelen = sizeof(long);
            if(format[nn] == 'l')
            {
                bytelen = sizeof(long long);
                nn += 1;
            }
            c = format[nn++];
            break;
        case 'z':
            bytelen = sizeof(size_t);
            c = format[nn++];
            break;
        case 't':
            bytelen = sizeof(ptrdiff_t);
            c = format[nn++];
            break;
        default:
            ;
        }
        
        //conversion specifier
        const char* str = buffer;
        if(c == 's')
        {
            //string
            str = va_arg(args, const char *);
            if (str == NULL)
            {
                str = "(null)";
            }
        }
        else if(c == 'c')
        {
            //character
            //NOTE: char is promoted to int when passed through the stack
            buffer[0] = (char)(va_arg(args, int));
            buffer[1] = '\0';
        }
        else if(c == 'p')
        {
            uint64_t value = (uintptr_t)(va_arg(args, void*));
            buffer[0] = '0';
            buffer[1] = 'x';
            xcc_fmt_format_integer(buffer + 2, sizeof(buffer) - 2, value, 'x');
        }
        else if (c == 'd' || c == 'i' || c == 'o' || c == 'u' || c == 'x' || c == 'X')
        {
            //integers - first read value from stack
            uint64_t value;
            int is_signed = (c == 'd' || c == 'i' || c == 'o');
            //NOTE: int8_t and int16_t are promoted to int when passed through the stack
            switch(bytelen)
            {
            case 1:
                value = (uint8_t)(va_arg(args, int));
                break;
            case 2:
                value = (uint16_t)(va_arg(args, int));
                break;
            case 4:
                value = va_arg(args, uint32_t);
                break;
            case 8:
                value = va_arg(args, uint64_t);
                break;
            default:
                return; //should not happen
            }
            //sign extension, if needed
            if(is_signed)
            {
                int shift = (int)(64 - 8 * bytelen);
                value = (uint64_t)(((int64_t)(value << shift)) >> shift);
            }
            //format the number properly into our buffer
            xcc_fmt_format_integer(buffer, sizeof(buffer), value, c);
        }
        else if (c == '%')
        {
            buffer[0] = '%';
            buffer[1] = '\0';
        }
        else
        {
            //__assert(__FILE__, __LINE__, "conversion specifier unsupported");
            return;
        }
        
        //if we are here, 'str' points to the content that must be outputted.
        //handle padding and alignment now
        slen = (int)strlen(str);
        if (sign != '\0' || prec != -1)
        {
            //__assert(__FILE__, __LINE__, "sign/precision unsupported");
            return;
        }
        if (slen < width && !padLeft)
        {
            char padChar = padZero ? '0' : ' ';
            xcc_fmt_stream_send_repeat(self, padChar, width - slen);
        }
        xcc_fmt_stream_send(self, str, slen);
        if (slen < width && padLeft)
        {
            char padChar = padZero ? '0' : ' ';
            xcc_fmt_stream_send_repeat(self, padChar, width - slen);
        }
    }
}

size_t xcc_fmt_vsnprintf(char *buffer, size_t buffer_size, const char *format, va_list args)
{
    xcc_fmt_stream_t stream;
    xcc_fmt_stream_init(&stream, buffer, buffer_size);
    xcc_fmt_stream_vformat(&stream, format, args);
    return xcc_fmt_stream_total(&stream);
}

size_t xcc_fmt_snprintf(char *buffer, size_t buffer_size, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    size_t buffer_len = xcc_fmt_vsnprintf(buffer, buffer_size, format, args);
    va_end(args);
    return buffer_len;
}
