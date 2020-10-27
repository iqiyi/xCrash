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

typedef int make_iso_compilers_happy;

#ifdef __arm__

#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include "xcc_errno.h"
#include "xcd_arm_exidx.h"
#include "xcd_regs.h"
#include "xcd_memory.h"
#include "xcd_util.h"
#include "xcd_log.h"

#define XCD_ARM_EXIDX_REGS_SP 13
#define XCD_ARM_EXIDX_REGS_PC 15

#define XCD_ARM_EXIDX_OP_FINISH 0xb0

typedef struct
{
    xcd_regs_t   *regs;
    xcd_memory_t *memory;
    pid_t         pid;

    uintptr_t     vsp;
    uintptr_t     pc;
    int           pc_set;
    
    size_t        exidx_offset;
    size_t        exidx_size;

    size_t        entry_offset;
    uint8_t       entry[32]; //same as libunwind
    size_t        entry_size;
    size_t        entry_idx;

    int           no_unwind;
    int           finished;
} xcd_arm_exidx_t;

static void xcd_arm_exidx_init(xcd_arm_exidx_t *self, xcd_regs_t *regs, xcd_memory_t *memory,
                               pid_t pid, size_t exidx_offset, size_t exidx_size, uintptr_t pc)
{
    self->regs = regs;
    self->memory = memory;
    self->pid = pid;
    
    self->vsp = xcd_regs_get_sp(regs);
    self->pc = pc;
    self->pc_set = 0;
    
    self->exidx_offset = exidx_offset;
    self->exidx_size = exidx_size;
    
    self->entry_offset = 0;
    self->entry_size = 0;
    self->entry_idx = 0;

    self->no_unwind = 0;
    self->finished = 0;
}

static int xcd_arm_exidx_prel31_addr(xcd_memory_t *memory, uint32_t offset, uint32_t* addr)
{
    uint32_t data;
    if(0 != xcd_memory_read_fully(memory, offset, &data, sizeof(data))) return XCC_ERRNO_MEM;

    //sign extend the value if necessary
    int32_t value = ((int32_t)(data) << 1) >> 1;
    *addr = (uint32_t)((int32_t)offset + value);
    
    return 0;
}

static int xcd_arm_exidx_entry_push(xcd_arm_exidx_t *self, uint8_t byte)
{
    if(self->entry_size >= 32) return XCC_ERRNO_NOSPACE;
    
    self->entry[self->entry_size] = byte;
    self->entry_size++;
    return 0;
}

static int xcd_arm_exidx_entry_pop(xcd_arm_exidx_t *self, uint8_t *byte)
{
    if(self->entry_idx >= self->entry_size) return XCC_ERRNO_NOTFND;

    *byte = self->entry[self->entry_idx];
    self->entry_idx++;
    return 0;
}

static int xcd_arm_exidx_get_entry_offset(xcd_arm_exidx_t *self)
{
    if(0 == self->exidx_offset || 0 == self->exidx_size) return XCC_ERRNO_NOTFND;

    size_t   first = 0;
    size_t   last = self->exidx_size / 8;
    size_t   current;
    uint32_t addr;
    int      r;
    
    while (first < last)
    {
        current = (first + last) / 2;

        if(0 != (r = xcd_arm_exidx_prel31_addr(self->memory, self->exidx_offset + current * 8, &addr))) return r;

        if(self->pc == addr)
        {
            self->entry_offset = self->exidx_offset + current * 8;
            return 0; //found
        }
        else if(self->pc < addr)
        {
            last = current;
        }
        else
        {
            first = current + 1;
        }
    }
    
    if(last > 0)
    {
        self->entry_offset = self->exidx_offset + (last - 1) * 8;
        return 0; //found
    }
    
    return XCC_ERRNO_NOTFND;
}

static int xcd_arm_exidx_get_entry(xcd_arm_exidx_t *self)
{
    int r;
    
    if(self->entry_offset & 1) return XCC_ERRNO_FORMAT;

    //read entry value
    uint32_t data;
    if(0 != xcd_memory_read_fully(self->memory, self->entry_offset + 4, &data, sizeof(data))) return XCC_ERRNO_MEM;
    
    if(1 == data)
    {
        //cantunwind
        self->no_unwind = 1;
        return XCC_ERRNO_FORMAT;
    }
    else if(data & (1UL << 31))
    {
        //a table entry itself (compact model: Su16)
        if((data >> 24) & 0xf) return XCC_ERRNO_FORMAT;
        if(0 != (r = xcd_arm_exidx_entry_push(self, (data >> 16) & 0xff))) return r;
        if(0 != (r = xcd_arm_exidx_entry_push(self, (data >> 8) & 0xff))) return r;
        if(0 != (r = xcd_arm_exidx_entry_push(self, data & 0xff))) return r;
        if((data & 0xff) != XCD_ARM_EXIDX_OP_FINISH)
            if(0 != (r = xcd_arm_exidx_entry_push(self, XCD_ARM_EXIDX_OP_FINISH))) return r;
        return 0; //finished
    }
    else
    {
        //a prel31 offset of the start of the table entry
        
        //get the address of the table entry
        int32_t signed_data = (int32_t)(data << 1) >> 1;
        uint32_t addr = (uint32_t)((int32_t)(self->entry_offset + 4) + signed_data);

        //get the table entry
        if(0 != xcd_memory_read_fully(self->memory, addr, &data, sizeof(data))) return XCC_ERRNO_MEM;

        size_t num_table_words = 0;
        if (data & (1UL << 31))
        {
            //compact model
            switch((data >> 24) & 0xf)
            {
            case 0:
                //Su16
                if(0 != (r = xcd_arm_exidx_entry_push(self, (data >> 16) & 0xff))) return r;
                if(0 != (r = xcd_arm_exidx_entry_push(self, (data >> 8) & 0xff))) return r;
                if(0 != (r = xcd_arm_exidx_entry_push(self, data & 0xff))) return r;
                if((data & 0xff) != XCD_ARM_EXIDX_OP_FINISH)
                    if(0 != (r = xcd_arm_exidx_entry_push(self, XCD_ARM_EXIDX_OP_FINISH))) return r;
                return 0; //finished
            case 1:
                //Lu16
            case 2:
                //Lu32
                num_table_words = (data >> 16) & 0xff;
                if(0 != (r = xcd_arm_exidx_entry_push(self, (data >> 8) & 0xff))) return r;
                if(0 != (r = xcd_arm_exidx_entry_push(self, data & 0xff))) return r;
                addr += 4;
                break;
            default:
                return XCC_ERRNO_FORMAT;
            }
        }
        else
        {
            //generic model
            addr += 4; //skip the personality routine data (prs_fnc_offset)
            if(0 != xcd_memory_read_fully(self->memory, addr, &data, sizeof(data))) return XCC_ERRNO_MEM;
            num_table_words = (data >> 24) & 0xff;
            if(0 != (r = xcd_arm_exidx_entry_push(self, (data >> 16) & 0xff))) return r;
            if(0 != (r = xcd_arm_exidx_entry_push(self, (data >> 8) & 0xff))) return r;
            if(0 != (r = xcd_arm_exidx_entry_push(self, data & 0xff))) return r;
            addr += 4;
        }

        if(num_table_words > 5) return XCC_ERRNO_FORMAT;

        //get additional insn
        size_t j;
        for(j = 0; j < num_table_words; j++)
        {
            if(0 != xcd_memory_read_fully(self->memory, addr, &data, sizeof(data))) return XCC_ERRNO_MEM;
            if(0 != (r = xcd_arm_exidx_entry_push(self, (data >> 24) & 0xff))) return r;
            if(0 != (r = xcd_arm_exidx_entry_push(self, (data >> 16) & 0xff))) return r;
            if(0 != (r = xcd_arm_exidx_entry_push(self, (data >> 8) & 0xff))) return r;
            if(0 != (r = xcd_arm_exidx_entry_push(self, data & 0xff))) return r;
            addr += 4;
        }

        if(0 == self->entry_size) return XCC_ERRNO_MISSING;
        if(XCD_ARM_EXIDX_OP_FINISH != self->entry[self->entry_size - 1])
            if(0 != (r = xcd_arm_exidx_entry_push(self, XCD_ARM_EXIDX_OP_FINISH))) return r;
        return 0; //finished
    }
}

static int xcd_arm_exidx_decode_entry_10_00(xcd_arm_exidx_t *self, uint8_t byte)
{
    int      r;
    uint16_t bytes = (uint16_t)((byte & 0xf) << 8);

    if(0 != (r = xcd_arm_exidx_entry_pop(self, &byte))) return r;
    bytes |= byte;

    if(0 == bytes)
    {
        // 10000000 00000000: Refuse to unwind
        self->no_unwind = 1;
        return XCC_ERRNO_FORMAT;
    }
    else
    {
        // 1000iiii iiiiiiii: Pop up to 12 integer registers under masks {r15-r12}, {r11-r4}
        bytes <<= 4;
        
        size_t reg;
        for(reg = 4; reg < 16; reg++)
        {
            if(bytes & (1 << reg))
            {
#if XCD_ARM_EXIDX_DEBUG
                XCD_LOG_DEBUG("ARM_EXIDE: 1000, ptrace, reg=%zu, vsp=%x", reg, self->vsp);
#endif
                if(0 != xcd_util_ptrace_read_fully(self->pid, self->vsp, &(self->regs->r[reg]), sizeof(uint32_t))) return XCC_ERRNO_MEM;
                self->vsp += 4;
            }
        }

        if(bytes & (1 << XCD_ARM_EXIDX_REGS_SP))
            self->vsp = xcd_regs_get_sp(self->regs);
        
        if(bytes & (1 << XCD_ARM_EXIDX_REGS_PC))
            self->pc_set = 1;
        
        return 0;
    }
}

static int xcd_arm_exidx_decode_entry_10_01(xcd_arm_exidx_t *self, uint8_t byte)
{
    uint8_t bits = byte & 0xf;

    switch(bits)
    {
    case 13:
        // 10011101: Reserved as prefix for ARM register to register moves
        return XCC_ERRNO_FORMAT;
    case 15:
        // 10011111: Reserved as prefix for Intel Wireless MMX register to register moves
        return XCC_ERRNO_FORMAT;
    default:
        // 1001nnnn: Set vsp = r[nnnn]
        self->vsp = self->regs->r[bits];
        return 0;
    }
}

static int xcd_arm_exidx_decode_entry_10_10(xcd_arm_exidx_t *self, uint8_t byte)
{
    // 10100nnn: Pop r4-r[4+nnn]
    // 10101nnn: Pop r4-r[4+nnn], r14
    size_t i;
    for(i = 4; i <= (size_t)(4 + (byte & 0x7)); i++)
    {
#if XCD_ARM_EXIDX_DEBUG
        XCD_LOG_DEBUG("ARM_EXIDE: 1010, ptrace, reg=%zu, vsp=%x", i, self->vsp);
#endif
        if(0 != xcd_util_ptrace_read_fully(self->pid, self->vsp, &(self->regs->r[i]), sizeof(uint32_t))) return XCC_ERRNO_MEM;
        self->vsp += 4;
    }
    if(byte & 0x8)
    {
#if XCD_ARM_EXIDX_DEBUG
        XCD_LOG_DEBUG("ARM_EXIDE: 1010, ptrace, reg=%zu, vsp=%x", 14, self->vsp);
#endif
        if(0 != xcd_util_ptrace_read_fully(self->pid, self->vsp, &(self->regs->r[14]), sizeof(uint32_t))) return XCC_ERRNO_MEM;
        self->vsp += 4;
    }
    return 0;
}

static int xcd_arm_exidx_decode_entry_10_11_0000(xcd_arm_exidx_t *self)
{
    // 10110000: Finish
    self->finished = 1;
    return XCC_ERRNO_RANGE;
}

static int xcd_arm_exidx_decode_entry_10_11_0001(xcd_arm_exidx_t *self)
{
    int     r;
    uint8_t byte;

    if(0 != (r = xcd_arm_exidx_entry_pop(self, &byte))) return r;

    if(0 == byte)
    {
        // 10110001 00000000: Spare
        return XCC_ERRNO_FORMAT;
    }
    else if(byte >> 4)
    {
        // 10110001 xxxxyyyy: Spare (xxxx != 0000)
        return XCC_ERRNO_FORMAT;        
    }
    else
    {
        // 10110001 0000iiii: Pop integer registers under mask {r3, r2, r1, r0}
        size_t reg;
        for(reg = 0; reg < 4; reg++)
        {
            if(byte & (1 << reg))
            {
#if XCD_ARM_EXIDX_DEBUG
                XCD_LOG_DEBUG("ARM_EXIDE: 10110001, ptrace, reg=%zu, vsp=%x", reg, self->vsp);
#endif
                if(0 != xcd_util_ptrace_read_fully(self->pid, self->vsp, &(self->regs->r[reg]), sizeof(uint32_t))) return XCC_ERRNO_MEM;
                self->vsp += 4;
            }
        }
        
    }
    
    return 0;
}

static int xcd_arm_exidx_decode_entry_10_11_0010(xcd_arm_exidx_t *self)
{
    // 10110010 uleb128: vsp = vsp + 0x204 + (uleb128 << 2)
    uint32_t result = 0;
    uint32_t shift = 0;
    uint8_t  byte;
    int      r;
    
    do
    {
        if(0 != (r = xcd_arm_exidx_entry_pop(self, &byte))) return r;
        result |= (uint32_t)((byte & 0x7f) << shift);
        shift += 7;
    } while (byte & 0x80);
    
    self->vsp += 0x204 + (result << 2);
    
    return 0;
}

static int xcd_arm_exidx_decode_entry_10_11_0011(xcd_arm_exidx_t *self)
{
    // 10110011 sssscccc: Pop VFP double precision registers D[ssss]-D[ssss+cccc] by FSTMFDX
    uint8_t byte;
    int     r;

    if(0 != (r = xcd_arm_exidx_entry_pop(self, &byte))) return r;

    self->vsp += ((byte & 0xf) + 1) * 8 + 4;

    return 0;
}

static int xcd_arm_exidx_decode_entry_10_11_1nnn(xcd_arm_exidx_t *self, uint8_t byte)
{
    // 10111nnn: Pop VFP double-precision registers D[8]-D[8+nnn] by FSTMFDX
    self->vsp += ((byte & 0x7) + 1) * 8 + 4;

    return 0;
}

static int xcd_arm_exidx_decode_entry_10_11_01nn(xcd_arm_exidx_t *self)
{
    (void)self;
    
    // 101101nn: Spare
    return XCC_ERRNO_FORMAT;
}

static int xcd_arm_exidx_decode_entry_10(xcd_arm_exidx_t *self, uint8_t byte)
{
    switch((byte >> 4) & 0x3)
    {
    case 0:
        return xcd_arm_exidx_decode_entry_10_00(self, byte);
    case 1:
        return xcd_arm_exidx_decode_entry_10_01(self, byte);
    case 2:
        return xcd_arm_exidx_decode_entry_10_10(self, byte);
    default:
        switch (byte & 0xf)
        {
        case 0:
            return xcd_arm_exidx_decode_entry_10_11_0000(self);
        case 1:
            return xcd_arm_exidx_decode_entry_10_11_0001(self);
        case 2:
            return xcd_arm_exidx_decode_entry_10_11_0010(self);
        case 3:
            return xcd_arm_exidx_decode_entry_10_11_0011(self);
        default:
            if(byte & 0x8)
            {
                return xcd_arm_exidx_decode_entry_10_11_1nnn(self, byte);
            }
            else
            {
                return xcd_arm_exidx_decode_entry_10_11_01nn(self);
            }
        }
    }
}

static int xcd_arm_exidx_decode_entry_11_000(xcd_arm_exidx_t *self, uint8_t byte)
{
    int     r;
    uint8_t bits = byte & 0x7;
    
    if(6 == bits)
    {
        if(0 != (r = xcd_arm_exidx_entry_pop(self, &byte))) return r;
        
        // 11000110 sssscccc: Intel Wireless MMX pop wR[ssss]-wR[ssss+cccc]
        self->vsp += ((byte & 0xf) + 1) * 8;
        return 0;
    }
    else if(7 == bits)
    {
        if(0 != (r = xcd_arm_exidx_entry_pop(self, &byte))) return r;

        if(0 == byte)
        {
            // 11000111 00000000: Spare
            return XCC_ERRNO_FORMAT;
        }
        else if(0 == (byte >> 4))
        {
            // 11000111 0000iiii: Intel Wireless MMX pop wCGR registers {wCGR0,1,2,3}
            self->vsp += (uintptr_t)(__builtin_popcount(byte) * 4);
            return 0;
        }
        else
        {
            // 11000111 xxxxyyyy: Spare (xxxx != 0000)
            return XCC_ERRNO_FORMAT;
        }
    }
    else
    {
        // 11000nnn: Intel Wireless MMX pop wR[10]-wR[10+nnn] (nnn != 6, 7)
        self->vsp += ((byte & 0x7) + 1) * 8;
        return 0;
    }
}

static int xcd_arm_exidx_decode_entry_11_001(xcd_arm_exidx_t *self, uint8_t byte)
{
    int     r;
    uint8_t bits = byte & 0x7;

    if(0 == bits)
    {
        if(0 != (r = xcd_arm_exidx_entry_pop(self, &byte))) return r;

        // 11001000 sssscccc: Pop VFP double precision registers D[16+ssss]-D[16+ssss+cccc] by VPUSH
        self->vsp += ((byte & 0xf) + 1) * 8;
        return 0;
    }
    else if(1 == bits)
    {
        if(0 != (r = xcd_arm_exidx_entry_pop(self, &byte))) return r;

        // 11001001 sssscccc: Pop VFP double precision registers D[ssss]-D[ssss+cccc] by VPUSH
        self->vsp += ((byte & 0xf) + 1) * 8;
        return 0;
    }
    else
    {
        // 11001yyy: Spare (yyy != 000, 001)
        return XCC_ERRNO_FORMAT;
    }
}

static int xcd_arm_exidx_decode_entry_11_010(xcd_arm_exidx_t *self, uint8_t byte)
{
    // 11010nnn: Pop VFP double precision registers D[8]-D[8+nnn] by VPUSH
    self->vsp += ((byte & 0x7) + 1) * 8;
    return 0;
}

static int xcd_arm_exidx_decode_entry_11(xcd_arm_exidx_t *self, uint8_t byte)
{
    switch ((byte >> 3) & 0x7)
    {
    case 0:
        return xcd_arm_exidx_decode_entry_11_000(self, byte);
    case 1:
        return xcd_arm_exidx_decode_entry_11_001(self, byte);
    case 2:
        return xcd_arm_exidx_decode_entry_11_010(self, byte);
    default:
        // 11xxxyyy: Spare (xxx != 000, 001, 010)
        return XCC_ERRNO_FORMAT;
    }
}

static int xcd_arm_exidx_decode_entry(xcd_arm_exidx_t *self)
{
    int r = 0;
    uint8_t byte;

    if(0 != (r = xcd_arm_exidx_entry_pop(self, &byte))) return r;

    switch(byte >> 6)
    {
    case 0:
        //00xxxxxx: vsp = vsp + (xxxxxx << 2) + 4
        self->vsp += (uintptr_t)(((byte & 0x3f) << 2) + 4);
        return 0;
    case 1:
        //01xxxxxx: vsp = vsp - (xxxxxx << 2) - 4
        self->vsp -= (uintptr_t)(((byte & 0x3f) << 2) + 4);
        return 0;
    case 2:
        return xcd_arm_exidx_decode_entry_10(self, byte);
    default:
        return xcd_arm_exidx_decode_entry_11(self, byte);
    }
}

int xcd_arm_exidx_step(xcd_regs_t *regs, xcd_memory_t *memory, pid_t pid,
                       size_t exidx_offset, size_t exidx_size, uintptr_t load_bias,
                       uintptr_t pc, int *finished)
{
    xcd_arm_exidx_t self;
    int             r;

    if(pc < load_bias) return XCC_ERRNO_NOTFND;

    pc -= load_bias;
    xcd_arm_exidx_init(&self, regs, memory, pid, exidx_offset, exidx_size, pc);
    
    //get entry offset
    if(0 != (r = xcd_arm_exidx_get_entry_offset(&self))) return r;

    //get entry
    if(0 != (r = xcd_arm_exidx_get_entry(&self)))
    {
        if(self.no_unwind)
        {
            *finished = 1;
            return 0;
        }
        return r;
    }
#if XCD_ARM_EXIDX_DEBUG
    size_t i;
    for(i = 0; i < self.entry_size; i++)
        XCD_LOG_DEBUG("ARM_EXIDX: entry[%zu]=%x", i, self.entry[i]);
#endif

    //decode entry
    while(0 == xcd_arm_exidx_decode_entry(&self));
    if(self.no_unwind)
    {
        *finished = 1;
        return 0;
    }

    //step the sp and pc
    if(self.finished)
    {
        if(0 == self.pc_set)
        {
            xcd_regs_set_pc_from_lr(self.regs, self.pid);
        }

        xcd_regs_set_sp(self.regs, self.vsp);
        
        //if the pc was set to zero, consider this the final frame
        *finished = (0 == xcd_regs_get_pc(self.regs) ? 1 : 0);

        return 0;
    }
    else
    {
        return XCC_ERRNO_UNKNOWN;
    }
}

#endif
