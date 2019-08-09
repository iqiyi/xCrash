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

// .eh_frame & .eh_frame_hdr:
// http://refspecs.linuxbase.org/LSB_5.0.0/LSB-Core-generic/LSB-Core-generic/ehframechpt.html

#include <stdint.h>
#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include "queue.h"
#include "tree.h"
#include "xcc_errno.h"
#include "xcd_dwarf.h"
#include "xcd_memory.h"
#include "xcd_regs.h"
#include "xcd_log.h"
#include "xcd_util.h"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpadded"

//DWARF data encoding
#define DW_EH_PE_absptr   0x00
#define DW_EH_PE_uleb128  0x01
#define DW_EH_PE_udata2   0x02
#define DW_EH_PE_udata4   0x03
#define DW_EH_PE_udata8   0x04
#define DW_EH_PE_sleb128  0x09
#define DW_EH_PE_sdata2   0x0A
#define DW_EH_PE_sdata4   0x0B
#define DW_EH_PE_sdata8   0x0C
#define DW_EH_PE_udata1   0x0D
#define DW_EH_PE_sdata1   0x0E
#define DW_EH_PE_block    0x0F
#define DW_EH_PE_pcrel    0x10
#define DW_EH_PE_textrel  0x20
#define DW_EH_PE_datarel  0x30
#define DW_EH_PE_funcrel  0x40
#define DW_EH_PE_aligned  0x50
//#define DW_EH_PE_indirect 0x80
#define DW_EH_PE_omit     0xFF

//CIE
typedef struct xcd_dwarf_cie
{
    size_t   offset;
    uint8_t  fde_address_encoding;
    uint8_t  segment_size;
    char     augmentation_string[8]; //should be enough. legal characters are: 'z', 'P', 'L', 'R', '\0'
    uint64_t cfa_instructions_offset;
    uint64_t cfa_instructions_end;
    uint64_t code_alignment_factor;
    int64_t  data_alignment_factor;
    uint64_t return_address_register;
    RB_ENTRY(xcd_dwarf_cie) link;
} xcd_dwarf_cie_t;
static int xcd_dwarf_cie_cmp(xcd_dwarf_cie_t *a, xcd_dwarf_cie_t *b)
{
    if(a->offset == b->offset) return 0;
    else return (a->offset > b->offset ? 1 : -1);
}
typedef RB_HEAD(xcd_dwarf_cie_tree, xcd_dwarf_cie) xcd_dwarf_cie_tree_t;
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-function"
RB_GENERATE_STATIC(xcd_dwarf_cie_tree, xcd_dwarf_cie, link, xcd_dwarf_cie_cmp)
#pragma clang diagnostic pop

//FDE
typedef struct
{
    uint64_t  cfa_instructions_offset;
    uint64_t  cfa_instructions_end;
    uintptr_t pc_start;
    uintptr_t pc_end;
    xcd_dwarf_cie_t *cie;
} xcd_dwarf_fde_t;

//DWARF object
struct xcd_dwarf
{
    xcd_dwarf_type_t          type;
    pid_t                     pid;
    uintptr_t                 load_bias;
    xcd_dwarf_cie_tree_t      cie_cache;
    
    xcd_memory_t             *memory;
    size_t                    memory_cur_offset;
    size_t                    memory_pc_offset;
    size_t                    memory_data_offset;
    
    size_t                    pc_offset;

    //XCD_DWARF_TYPE_DEBUG_FRAME  mode: starting / ending offset of .debug_frame
    //XCD_DWARF_TYPE_EH_FRAME     mode: starting / ending offset of .eh_frame
    //XCD_DWARF_TYPE_EH_FRAME_HDR mode: starting / ending offset of binary search table in .eh_frame_hdr
    size_t                    entries_offset;
    size_t                    entries_end;

    //for XCD_DWARF_TYPE_EH_FRAME_HDR mode only
    size_t                    eh_frame_hdr_fde_count;
    uint8_t                   eh_frame_hdr_table_encoding;
    size_t                    eh_frame_hdr_table_entry_size;
};

//location rule type
#define DW_LOC_INVALID        0
#define DW_LOC_UNDEFINED      1
#define DW_LOC_OFFSET         2
#define DW_LOC_VAL_OFFSET     3
#define DW_LOC_REGISTER       4
#define DW_LOC_EXPRESSION     5
#define DW_LOC_VAL_EXPRESSION 6

//location rule
#define XCD_DWARF_REG_NUM 0xFFFF
typedef struct xcd_dwarf_loc_rule
{
    uint8_t  type;
    uint64_t values[2];
} xcd_dwarf_loc_rule_t;

//location
typedef struct
{
    xcd_dwarf_loc_rule_t cfa_rule;
    xcd_dwarf_loc_rule_t reg_rules[XCD_DWARF_REG_NUM];
} xcd_dwarf_loc_t;

//location stack
typedef struct xcd_dwarf_loc_node
{
    xcd_dwarf_loc_t loc;
    TAILQ_ENTRY(xcd_dwarf_loc_node,) link;
} xcd_dwarf_loc_node_t;
typedef TAILQ_HEAD(xcd_dwarf_loc_node_stack, xcd_dwarf_loc_node,) xcd_dwarf_loc_node_stack_t;

//cfa
typedef struct
{
    uint8_t illegal;
    uint8_t operand_types[2];
} xcd_dwarf_cfa_t;
static xcd_dwarf_cfa_t xcd_dwarf_cfa_table[64] = {
    /* 0x00 */ {0, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0x01 */ {0, {DW_EH_PE_absptr,  DW_EH_PE_omit}},
    /* 0x02 */ {0, {DW_EH_PE_udata1,  DW_EH_PE_omit}},
    /* 0x03 */ {0, {DW_EH_PE_udata2,  DW_EH_PE_omit}},
    /* 0x04 */ {0, {DW_EH_PE_udata4,  DW_EH_PE_omit}},
    /* 0x05 */ {0, {DW_EH_PE_uleb128, DW_EH_PE_uleb128}},
    /* 0x06 */ {0, {DW_EH_PE_uleb128, DW_EH_PE_omit}},
    /* 0x07 */ {0, {DW_EH_PE_uleb128, DW_EH_PE_omit}},
    /* 0x08 */ {0, {DW_EH_PE_uleb128, DW_EH_PE_omit}},
    /* 0x09 */ {0, {DW_EH_PE_uleb128, DW_EH_PE_uleb128}},
    /* 0x0a */ {0, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0x0b */ {0, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0x0c */ {0, {DW_EH_PE_uleb128, DW_EH_PE_uleb128}},
    /* 0x0d */ {0, {DW_EH_PE_uleb128, DW_EH_PE_omit}},
    /* 0x0e */ {0, {DW_EH_PE_uleb128, DW_EH_PE_omit}},
    /* 0x0f */ {0, {DW_EH_PE_block,   DW_EH_PE_omit}},
    /* 0x10 */ {0, {DW_EH_PE_uleb128, DW_EH_PE_block}},
    /* 0x11 */ {0, {DW_EH_PE_uleb128, DW_EH_PE_sleb128}},
    /* 0x12 */ {0, {DW_EH_PE_uleb128, DW_EH_PE_sleb128}},
    /* 0x13 */ {0, {DW_EH_PE_sleb128, DW_EH_PE_omit}},
    /* 0x14 */ {0, {DW_EH_PE_uleb128, DW_EH_PE_uleb128}},
    /* 0x15 */ {0, {DW_EH_PE_uleb128, DW_EH_PE_sleb128}},
    /* 0x16 */ {0, {DW_EH_PE_uleb128, DW_EH_PE_block}},
    /* 0x17 */ {1, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0x18 */ {1, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0x19 */ {1, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0x1a */ {1, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0x1b */ {1, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0x1c */ {1, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0x1d */ {1, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0x1e */ {1, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0x1f */ {1, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0x20 */ {1, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0x21 */ {1, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0x22 */ {1, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0x23 */ {1, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0x24 */ {1, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0x25 */ {1, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0x26 */ {1, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0x27 */ {1, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0x28 */ {1, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0x29 */ {1, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0x2a */ {1, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0x2b */ {1, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0x2c */ {1, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0x2d */ {1, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0x2e */ {0, {DW_EH_PE_uleb128, DW_EH_PE_omit}},
    /* 0x2f */ {0, {DW_EH_PE_uleb128, DW_EH_PE_uleb128}},
    /* 0x30 */ {1, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0x31 */ {1, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0x32 */ {1, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0x33 */ {1, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0x34 */ {1, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0x35 */ {1, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0x36 */ {1, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0x37 */ {1, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0x38 */ {1, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0x39 */ {1, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0x3a */ {1, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0x3b */ {1, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0x3c */ {1, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0x3d */ {1, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0x3e */ {1, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0x3f */ {1, {DW_EH_PE_omit,    DW_EH_PE_omit}}
};

//op
typedef struct
{
    uint8_t illegal;
    uint8_t required_stack_values;
    uint8_t operand_types[2];
} xcd_dwarf_op_t;
static xcd_dwarf_op_t xcd_dwarf_op_table[256] = {
    /* 0x00 */ {1, 0, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0x01 */ {1, 0, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0x02 */ {1, 0, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0x03 */ {0, 0, {DW_EH_PE_absptr,  DW_EH_PE_omit}},
    /* 0x04 */ {1, 0, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0x05 */ {1, 0, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0x06 */ {0, 1, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0x07 */ {1, 0, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0x08 */ {0, 0, {DW_EH_PE_udata1,  DW_EH_PE_omit}},
    /* 0x09 */ {0, 0, {DW_EH_PE_sdata1,  DW_EH_PE_omit}},
    /* 0x0a */ {0, 0, {DW_EH_PE_udata2,  DW_EH_PE_omit}},
    /* 0x0b */ {0, 0, {DW_EH_PE_sdata2,  DW_EH_PE_omit}},
    /* 0x0c */ {0, 0, {DW_EH_PE_udata4,  DW_EH_PE_omit}},
    /* 0x0d */ {0, 0, {DW_EH_PE_sdata4,  DW_EH_PE_omit}},
    /* 0x0e */ {0, 0, {DW_EH_PE_udata8,  DW_EH_PE_omit}},
    /* 0x0f */ {0, 0, {DW_EH_PE_sdata8,  DW_EH_PE_omit}},
    /* 0x10 */ {0, 0, {DW_EH_PE_uleb128, DW_EH_PE_omit}},
    /* 0x11 */ {0, 0, {DW_EH_PE_sleb128, DW_EH_PE_omit}},
    /* 0x12 */ {0, 1, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0x13 */ {0, 1, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0x14 */ {0, 2, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0x15 */ {0, 0, {DW_EH_PE_udata1,  DW_EH_PE_omit}},
    /* 0x16 */ {0, 2, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0x17 */ {0, 3, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0x18 */ {1, 2, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0x19 */ {0, 1, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0x1a */ {0, 2, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0x1b */ {0, 2, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0x1c */ {0, 2, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0x1d */ {0, 2, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0x1e */ {0, 2, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0x1f */ {0, 1, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0x20 */ {0, 1, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0x21 */ {0, 2, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0x22 */ {0, 2, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0x23 */ {0, 1, {DW_EH_PE_uleb128, DW_EH_PE_omit}},
    /* 0x24 */ {0, 2, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0x25 */ {0, 2, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0x26 */ {0, 2, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0x27 */ {0, 2, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0x28 */ {0, 1, {DW_EH_PE_sdata2,  DW_EH_PE_omit}},
    /* 0x29 */ {0, 2, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0x2a */ {0, 2, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0x2b */ {0, 2, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0x2c */ {0, 2, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0x2d */ {0, 2, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0x2e */ {0, 2, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0x2f */ {0, 0, {DW_EH_PE_sdata2,  DW_EH_PE_omit}},
    /* 0x30 */ {0, 0, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0x31 */ {0, 0, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0x32 */ {0, 0, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0x33 */ {0, 0, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0x34 */ {0, 0, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0x35 */ {0, 0, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0x36 */ {0, 0, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0x37 */ {0, 0, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0x38 */ {0, 0, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0x39 */ {0, 0, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0x3a */ {0, 0, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0x3b */ {0, 0, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0x3c */ {0, 0, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0x3d */ {0, 0, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0x3e */ {0, 0, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0x3f */ {0, 0, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0x40 */ {0, 0, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0x41 */ {0, 0, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0x42 */ {0, 0, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0x43 */ {0, 0, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0x44 */ {0, 0, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0x45 */ {0, 0, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0x46 */ {0, 0, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0x47 */ {0, 0, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0x48 */ {0, 0, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0x49 */ {0, 0, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0x4a */ {0, 0, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0x4b */ {0, 0, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0x4c */ {0, 0, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0x4d */ {0, 0, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0x4e */ {0, 0, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0x4f */ {0, 0, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0x50 */ {1, 0, {DW_EH_PE_omit,    DW_EH_PE_omit}}, //don't support DW_OP_reg0 ~ DW_OP_reg31
    /* 0x51 */ {1, 0, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0x52 */ {1, 0, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0x53 */ {1, 0, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0x54 */ {1, 0, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0x55 */ {1, 0, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0x56 */ {1, 0, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0x57 */ {1, 0, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0x58 */ {1, 0, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0x59 */ {1, 0, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0x5a */ {1, 0, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0x5b */ {1, 0, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0x5c */ {1, 0, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0x5d */ {1, 0, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0x5e */ {1, 0, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0x5f */ {1, 0, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0x60 */ {1, 0, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0x61 */ {1, 0, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0x62 */ {1, 0, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0x63 */ {1, 0, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0x64 */ {1, 0, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0x65 */ {1, 0, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0x66 */ {1, 0, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0x67 */ {1, 0, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0x68 */ {1, 0, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0x69 */ {1, 0, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0x6a */ {1, 0, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0x6b */ {1, 0, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0x6c */ {1, 0, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0x6d */ {1, 0, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0x6e */ {1, 0, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0x6f */ {1, 0, {DW_EH_PE_omit,    DW_EH_PE_omit}}, //don't support DW_OP_reg31
    /* 0x70 */ {0, 0, {DW_EH_PE_sleb128, DW_EH_PE_omit}},
    /* 0x71 */ {0, 0, {DW_EH_PE_sleb128, DW_EH_PE_omit}},
    /* 0x72 */ {0, 0, {DW_EH_PE_sleb128, DW_EH_PE_omit}},
    /* 0x73 */ {0, 0, {DW_EH_PE_sleb128, DW_EH_PE_omit}},
    /* 0x74 */ {0, 0, {DW_EH_PE_sleb128, DW_EH_PE_omit}},
    /* 0x75 */ {0, 0, {DW_EH_PE_sleb128, DW_EH_PE_omit}},
    /* 0x76 */ {0, 0, {DW_EH_PE_sleb128, DW_EH_PE_omit}},
    /* 0x77 */ {0, 0, {DW_EH_PE_sleb128, DW_EH_PE_omit}},
    /* 0x78 */ {0, 0, {DW_EH_PE_sleb128, DW_EH_PE_omit}},
    /* 0x79 */ {0, 0, {DW_EH_PE_sleb128, DW_EH_PE_omit}},
    /* 0x7a */ {0, 0, {DW_EH_PE_sleb128, DW_EH_PE_omit}},
    /* 0x7b */ {0, 0, {DW_EH_PE_sleb128, DW_EH_PE_omit}},
    /* 0x7c */ {0, 0, {DW_EH_PE_sleb128, DW_EH_PE_omit}},
    /* 0x7d */ {0, 0, {DW_EH_PE_sleb128, DW_EH_PE_omit}},
    /* 0x7e */ {0, 0, {DW_EH_PE_sleb128, DW_EH_PE_omit}},
    /* 0x7f */ {0, 0, {DW_EH_PE_sleb128, DW_EH_PE_omit}},
    /* 0x80 */ {0, 0, {DW_EH_PE_sleb128, DW_EH_PE_omit}},
    /* 0x81 */ {0, 0, {DW_EH_PE_sleb128, DW_EH_PE_omit}},
    /* 0x82 */ {0, 0, {DW_EH_PE_sleb128, DW_EH_PE_omit}},
    /* 0x83 */ {0, 0, {DW_EH_PE_sleb128, DW_EH_PE_omit}},
    /* 0x84 */ {0, 0, {DW_EH_PE_sleb128, DW_EH_PE_omit}},
    /* 0x85 */ {0, 0, {DW_EH_PE_sleb128, DW_EH_PE_omit}},
    /* 0x86 */ {0, 0, {DW_EH_PE_sleb128, DW_EH_PE_omit}},
    /* 0x87 */ {0, 0, {DW_EH_PE_sleb128, DW_EH_PE_omit}},
    /* 0x88 */ {0, 0, {DW_EH_PE_sleb128, DW_EH_PE_omit}},
    /* 0x89 */ {0, 0, {DW_EH_PE_sleb128, DW_EH_PE_omit}},
    /* 0x8a */ {0, 0, {DW_EH_PE_sleb128, DW_EH_PE_omit}},
    /* 0x8b */ {0, 0, {DW_EH_PE_sleb128, DW_EH_PE_omit}},
    /* 0x8c */ {0, 0, {DW_EH_PE_sleb128, DW_EH_PE_omit}},
    /* 0x8d */ {0, 0, {DW_EH_PE_sleb128, DW_EH_PE_omit}},
    /* 0x8e */ {0, 0, {DW_EH_PE_sleb128, DW_EH_PE_omit}},
    /* 0x8f */ {0, 0, {DW_EH_PE_sleb128, DW_EH_PE_omit}},
    /* 0x90 */ {1, 0, {DW_EH_PE_uleb128, DW_EH_PE_omit}}, //don't support DW_OP_regx
    /* 0x91 */ {1, 0, {DW_EH_PE_sleb128, DW_EH_PE_omit}},
    /* 0x92 */ {0, 0, {DW_EH_PE_uleb128, DW_EH_PE_sleb128}},
    /* 0x93 */ {1, 0, {DW_EH_PE_uleb128, DW_EH_PE_omit}},
    /* 0x94 */ {0, 1, {DW_EH_PE_udata1,  DW_EH_PE_omit}},
    /* 0x95 */ {1, 0, {DW_EH_PE_udata1,  DW_EH_PE_omit}},
    /* 0x96 */ {0, 0, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0x97 */ {1, 0, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0x98 */ {1, 0, {DW_EH_PE_udata2,  DW_EH_PE_omit}},
    /* 0x99 */ {1, 0, {DW_EH_PE_udata4,  DW_EH_PE_omit}},
    /* 0x9a */ {1, 0, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0x9b */ {1, 0, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0x9c */ {1, 0, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0x9d */ {1, 0, {DW_EH_PE_uleb128, DW_EH_PE_uleb128}},
    /* 0x9e */ {1, 0, {DW_EH_PE_uleb128, DW_EH_PE_omit}},
    /* 0x9f */ {1, 1, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0xa0 */ {1, 0, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0xa1 */ {1, 0, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0xa2 */ {1, 0, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0xa3 */ {1, 0, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0xa4 */ {1, 0, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0xa5 */ {1, 0, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0xa6 */ {1, 0, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0xa7 */ {1, 0, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0xa8 */ {1, 0, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0xa9 */ {1, 0, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0xaa */ {1, 0, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0xab */ {1, 0, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0xac */ {1, 0, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0xad */ {1, 0, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0xae */ {1, 0, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0xaf */ {1, 0, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0xb0 */ {1, 0, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0xb1 */ {1, 0, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0xb2 */ {1, 0, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0xb3 */ {1, 0, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0xb4 */ {1, 0, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0xb5 */ {1, 0, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0xb6 */ {1, 0, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0xb7 */ {1, 0, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0xb8 */ {1, 0, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0xb9 */ {1, 0, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0xba */ {1, 0, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0xbb */ {1, 0, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0xbc */ {1, 0, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0xbd */ {1, 0, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0xbe */ {1, 0, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0xbf */ {1, 0, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0xc0 */ {1, 0, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0xc1 */ {1, 0, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0xc2 */ {1, 0, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0xc3 */ {1, 0, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0xc4 */ {1, 0, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0xc5 */ {1, 0, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0xc6 */ {1, 0, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0xc7 */ {1, 0, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0xc8 */ {1, 0, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0xc9 */ {1, 0, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0xca */ {1, 0, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0xcb */ {1, 0, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0xcc */ {1, 0, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0xcd */ {1, 0, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0xce */ {1, 0, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0xcf */ {1, 0, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0xd0 */ {1, 0, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0xd1 */ {1, 0, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0xd2 */ {1, 0, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0xd3 */ {1, 0, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0xd4 */ {1, 0, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0xd5 */ {1, 0, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0xd6 */ {1, 0, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0xd7 */ {1, 0, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0xd8 */ {1, 0, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0xd9 */ {1, 0, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0xda */ {1, 0, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0xdb */ {1, 0, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0xdc */ {1, 0, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0xdd */ {1, 0, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0xde */ {1, 0, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0xdf */ {1, 0, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0xe0 */ {1, 0, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0xe1 */ {1, 0, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0xe2 */ {1, 0, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0xe3 */ {1, 0, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0xe4 */ {1, 0, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0xe5 */ {1, 0, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0xe6 */ {1, 0, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0xe7 */ {1, 0, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0xe8 */ {1, 0, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0xe9 */ {1, 0, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0xea */ {1, 0, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0xeb */ {1, 0, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0xec */ {1, 0, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0xed */ {1, 0, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0xee */ {1, 0, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0xef */ {1, 0, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0xf0 */ {1, 0, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0xf1 */ {1, 0, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0xf2 */ {1, 0, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0xf3 */ {1, 0, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0xf4 */ {1, 0, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0xf5 */ {1, 0, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0xf6 */ {1, 0, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0xf7 */ {1, 0, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0xf8 */ {1, 0, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0xf9 */ {1, 0, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0xfa */ {1, 0, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0xfb */ {1, 0, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0xfc */ {1, 0, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0xfd */ {1, 0, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0xfe */ {1, 0, {DW_EH_PE_omit,    DW_EH_PE_omit}},
    /* 0xff */ {1, 0, {DW_EH_PE_omit,    DW_EH_PE_omit}}
};


//////////////////////////////////////////////////////////////////////
// utilities

static int xcd_dwarf_is_cie_64(xcd_dwarf_t *self, uint64_t v)
{
    if(XCD_DWARF_TYPE_DEBUG_FRAME == self->type)
        return (v == (uint64_t)(-1)) ? 1 : 0;
    else
        return (v == 0) ? 1 : 0;
}

static int xcd_dwarf_is_cie_32(xcd_dwarf_t *self, uint32_t v)
{
    if(XCD_DWARF_TYPE_DEBUG_FRAME == self->type)
        return (v == (uint32_t)(-1)) ? 1 : 0;
    else
        return (v == 0) ? 1 : 0;
}

static size_t xcd_dwarf_adjust_cie_offset(xcd_dwarf_t *self, size_t cur_field_offset, size_t v)
{
    if(XCD_DWARF_TYPE_DEBUG_FRAME == self->type)
        return self->entries_offset + v;
    else
        return cur_field_offset - v;
}

static uintptr_t xcd_dwarf_adjust_pc_from_fde(xcd_dwarf_t *self, size_t cur_field_offset, uintptr_t v)
{
    if(XCD_DWARF_TYPE_DEBUG_FRAME == self->type)
        return v;
    else
        return (uintptr_t)cur_field_offset + v;
}

static int xcd_dwarf_read_bytes(xcd_dwarf_t *self, void *value, size_t size)
{
    int r;
    
    if(0 != (r = xcd_memory_read_fully(self->memory, self->memory_cur_offset, value, size))) return r;
    self->memory_cur_offset += size;
    
    return 0;
}

static int xcd_dwarf_read_uleb128(xcd_dwarf_t *self, uint64_t *value)
{
    size_t i;
    int    r;
    
    if(0 != (r = xcd_memory_read_uleb128(self->memory, self->memory_cur_offset, value, &i))) return r;
    self->memory_cur_offset += i;
    
    return 0;
}

static int xcd_dwarf_read_sleb128(xcd_dwarf_t *self, int64_t *value)
{
    size_t i;
    int    r;

    if(0 != (r = xcd_memory_read_sleb128(self->memory, self->memory_cur_offset, value, &i))) return r;
    self->memory_cur_offset += i;
    
    return 0;
}

static int xcd_dwarf_read_encoded(xcd_dwarf_t *self, uint64_t *value, uint8_t encoding)
{
    int       r;
    uintptr_t vp;
    uint8_t   vu8;
    int8_t    vi8;
    uint16_t  vu16;
    int16_t   vi16;
    uint32_t  vu32;
    int32_t   vi32;
    uint64_t  vu64;
    int64_t   vi64;

    *value = 0;

    if(encoding == DW_EH_PE_omit) return 0;
    
    if(encoding == DW_EH_PE_aligned)
    {
        //check overflow
        if(__builtin_add_overflow(self->memory_cur_offset, sizeof(uintptr_t) - 1, &(self->memory_cur_offset))) return XCC_ERRNO_RANGE;

        //align
        self->memory_cur_offset &= -sizeof(uintptr_t);

        if(0 != (r = xcd_dwarf_read_bytes(self, &vp, sizeof(uintptr_t)))) return r;
        *value = (uint64_t)vp;
        return 0;
    }

    switch(encoding & 0x0f)
    {
    case DW_EH_PE_absptr:
        if(0 != (r = xcd_dwarf_read_bytes(self, &vp, sizeof(uintptr_t)))) return r;
        *value = (uint64_t)vp;
        break;
    case DW_EH_PE_uleb128:
        if(0 != (r = xcd_dwarf_read_uleb128(self, &vu64))) return r;
        *value = (uint64_t)vu64;
        break;
    case DW_EH_PE_sleb128:
        if(0 != (r = xcd_dwarf_read_sleb128(self, &vi64))) return r;
        *value = (uint64_t)vi64;
        break;
    case DW_EH_PE_udata1:
        if(0 != (r = xcd_dwarf_read_bytes(self, &vu8, 1))) return r;
        *value = (uint64_t)vu8;
        break;
    case DW_EH_PE_sdata1:
        if(0 != (r = xcd_dwarf_read_bytes(self, &vi8, 1))) return r;
        *value = (uint64_t)vi8;
        break;
    case DW_EH_PE_udata2:
        if(0 != (r = xcd_dwarf_read_bytes(self, &vu16, 2))) return r;
        *value = (uint64_t)vu16;
        break;
    case DW_EH_PE_sdata2:
        if(0 != (r = xcd_dwarf_read_bytes(self, &vi16, 2))) return r;
        *value = (uint64_t)vi16;
        break;
    case DW_EH_PE_udata4:
        if(0 != (r = xcd_dwarf_read_bytes(self, &vu32, 4))) return r;
        *value = (uint64_t)vu32;
        break;
    case DW_EH_PE_sdata4:
        if(0 != (r = xcd_dwarf_read_bytes(self, &vi32, 4))) return r;
        *value = (uint64_t)vi32;
        break;
    case DW_EH_PE_udata8:
        if(0 != (r = xcd_dwarf_read_bytes(self, &vu64, 8))) return r;
        *value = (uint64_t)vu64;
        break;
    case DW_EH_PE_sdata8:
        if(0 != (r = xcd_dwarf_read_bytes(self, &vi64, 8))) return r;
        *value = (uint64_t)vi64;
        break;
    default:
        return XCC_ERRNO_FORMAT;
    }

    switch(encoding & 0x70)
    {
    case DW_EH_PE_absptr:
        return 0;
    case DW_EH_PE_pcrel:
        if(((uintptr_t)-1) == self->memory_pc_offset) return XCC_ERRNO_NOTSPT;
        *value += self->memory_pc_offset;
        return 0;
    case DW_EH_PE_datarel:
        if(((uintptr_t)-1) == self->memory_data_offset) return XCC_ERRNO_NOTSPT;
        *value += self->memory_data_offset;
        return 0;
    case DW_EH_PE_textrel:
    case DW_EH_PE_funcrel:
        return XCC_ERRNO_NOTSPT;
    default:
        return XCC_ERRNO_FORMAT;
    }
}

//////////////////////////////////////////////////////////////////////
// get CIE

static xcd_dwarf_cie_t *xcd_dwarf_get_cie_from_offset(xcd_dwarf_t *self, size_t offset)
{
    xcd_dwarf_cie_t  cie_key = {.offset = offset};
    xcd_dwarf_cie_t *cie = NULL;
    uint8_t          cie_version;
    uint8_t          v8;
    uint32_t         v32;
    uint64_t         v64;
    size_t           i;
    
    self->memory_cur_offset = offset;

    //check cache
    if(NULL != (cie = RB_FIND(xcd_dwarf_cie_tree, &(self->cie_cache), &cie_key))) return cie;
    
    //create cie
    if(NULL == (cie = calloc(1, sizeof(xcd_dwarf_cie_t)))) goto err;
    cie->offset = offset; //key
    
    //get length
    if(0 != xcd_dwarf_read_bytes(self, &v32, 4)) goto err;

    if((uint32_t)(-1) == v32) //64bits DWARF FDE
    {
        //get extended length
        if(0 != xcd_dwarf_read_bytes(self, &v64, 8)) goto err;
        if(v64 > SIZE_MAX) goto err;

        cie->cfa_instructions_end = self->memory_cur_offset + (size_t)v64;
        cie->fde_address_encoding = DW_EH_PE_sdata8;
        
        //get CIE ID
        if(0 != xcd_dwarf_read_bytes(self, &v64, 8)) goto err;
        if(!xcd_dwarf_is_cie_64(self, v64)) goto err; //not a CIE
    }
    else //32bits DWARF FDE
    {
        cie->cfa_instructions_end = self->memory_cur_offset + (size_t)v32;
        cie->fde_address_encoding = DW_EH_PE_sdata4;
        
        //get CIE ID
        if(0 != xcd_dwarf_read_bytes(self, &v32, 4)) goto err;
        if(!xcd_dwarf_is_cie_32(self, v32)) goto err; //not a CIE
    }

    //check version
    if(0 != xcd_dwarf_read_bytes(self, &cie_version, 1)) goto err;
    if(1 != cie_version && 3 != cie_version && 4 != cie_version && 5 != cie_version) goto err;

    //get augmentation string
    for(i = 0; i < sizeof(cie->augmentation_string); i++)
    {
        if(0 != xcd_dwarf_read_bytes(self, &v8, 1)) goto err;
        cie->augmentation_string[i] = (char)v8;
        if('\0' == (char)v8) break;
    }
    if(i >= sizeof(cie->augmentation_string)) goto err; //augmentation string is too long

    if(4 == cie_version || 5 == cie_version)
    {
        //skip address size
        self->memory_cur_offset += 1;
        
        //get segment size
        if(0 != xcd_dwarf_read_bytes(self, &(cie->segment_size), 1)) goto err;
    }

    //get code alignment factor
    if(0 != xcd_dwarf_read_uleb128(self, &(cie->code_alignment_factor))) goto err;

    //get data alignment factor
    if(0 != xcd_dwarf_read_sleb128(self, &(cie->data_alignment_factor))) goto err;

    //get return address register
    if(1 == cie_version)
    {
        if(0 != xcd_dwarf_read_bytes(self, &v8, 1)) goto err;
        cie->return_address_register = (uint64_t)v8;
    }
    else
    {
        if(0 != xcd_dwarf_read_uleb128(self, &(cie->return_address_register))) goto err;
    }
    if(cie->return_address_register >= XCD_REGS_MACHINE_NUM) goto err;

    if('z' != cie->augmentation_string[0])
    {
        cie->cfa_instructions_offset = self->memory_cur_offset;
    }
    else
    {
        //get augmentation data length
        if(0 != xcd_dwarf_read_uleb128(self, &v64)) goto err;
        
        cie->cfa_instructions_offset = self->memory_cur_offset + (size_t)v64;

        for(i = 1; i < sizeof(cie->augmentation_string); i++)
        {
            switch(cie->augmentation_string[i])
            {
            case 'L':
                //skip LSDA encoding
                self->memory_cur_offset += 1;
                break;
            case 'R':
                //get FDE address encoding
                if(0 != xcd_dwarf_read_bytes(self, &(cie->fde_address_encoding), 1)) goto err;
                break;
            case 'P':
                //get personality routine encoding
                if(0 != xcd_dwarf_read_bytes(self, &v8, 1)) goto err;
                //skip personality routine
                self->memory_pc_offset = self->pc_offset;
                if(0 != xcd_dwarf_read_encoded(self, &v64, v8)) goto err;
                break;
            default:
                break;
            }
        }
    }
    
    RB_INSERT(xcd_dwarf_cie_tree, &(self->cie_cache), cie);
    return cie;

 err:
#if XCD_DWARF_DEBUG
    XCD_LOG_DEBUG("DWARF: get CIE failed, offset=%"PRIxPTR, offset);
#endif
    if(NULL != cie) free(cie);
    return NULL;
}

//////////////////////////////////////////////////////////////////////
// get FDE

static xcd_dwarf_fde_t *xcd_dwarf_get_fde_from_offset(xcd_dwarf_t *self, size_t *offset, uintptr_t pc)
{
    xcd_dwarf_fde_t *fde = NULL;
    xcd_dwarf_cie_t *cie;
    uint64_t         cfa_instructions_offset;
    uint64_t         cfa_instructions_end = self->entries_end;
    uintptr_t        pc_start;
    uintptr_t        pc_end;
    size_t           cur_offset;
    size_t           cie_offset;
    uint32_t         v32;
    uint64_t         v64;
    size_t           cur_field_offset;

    self->memory_cur_offset = *offset;

    //get length
    if(0 != xcd_dwarf_read_bytes(self, &v32, 4)) goto end;
    
    if((uint32_t)(-1) == v32) //64bits DWARF FDE
    {
        //get extended length
        if(0 != xcd_dwarf_read_bytes(self, &v64, 8)) goto end;
        if(v64 > SIZE_MAX) goto end;
        cfa_instructions_end = self->memory_cur_offset + (size_t)v64;

        //get CIE offset
        cur_field_offset = self->memory_cur_offset;
        if(0 != xcd_dwarf_read_bytes(self, &v64, 8)) goto end;
        if(xcd_dwarf_is_cie_64(self, v64)) goto end; //ignore cie
        if(v64 > SIZE_MAX) goto end;
        cie_offset = xcd_dwarf_adjust_cie_offset(self, cur_field_offset, (size_t)v64);
    }
    else //32bits DWARF FDE
    {
        cfa_instructions_end = self->memory_cur_offset + (size_t)v32;
        
        //get CIE offset
        cur_field_offset = self->memory_cur_offset;
        if(0 != xcd_dwarf_read_bytes(self, &v32, 4)) goto end;
        if(xcd_dwarf_is_cie_32(self, v32)) goto end; //ignore cie
        cie_offset = xcd_dwarf_adjust_cie_offset(self, cur_field_offset, (size_t)v32);
    }

    //get CIE
    cur_offset = self->memory_cur_offset;
    cie = xcd_dwarf_get_cie_from_offset(self, cie_offset);
    self->memory_cur_offset = cur_offset;
    if(NULL == cie) goto end;

    //skip segment selector
    self->memory_cur_offset += cie->segment_size;

    //get PC start
    cur_field_offset = self->memory_cur_offset;
    self->memory_pc_offset = self->load_bias;
    if(0 != xcd_dwarf_read_encoded(self, &v64, cie->fde_address_encoding)) goto end;
    pc_start = xcd_dwarf_adjust_pc_from_fde(self, cur_field_offset, (uintptr_t)v64);

    //get PC Range
    self->memory_pc_offset = 0; //PC Range is always an absolute value
    if(0 != xcd_dwarf_read_encoded(self, &v64, cie->fde_address_encoding)) goto end;

    //get PC end
    pc_end = pc_start + (uintptr_t)v64;

    //check current PC
    if(pc < pc_start || pc >= pc_end) goto end;

    if(cie->augmentation_string[0] == 'z')
    {
        //get augmentation data length
        if(0 != xcd_dwarf_read_uleb128(self, &v64)) goto end;

        //skip augmentation data
        self->memory_cur_offset += (size_t)v64;
    }

    //get CFA instructions offset
    cfa_instructions_offset = self->memory_cur_offset;
    if(cfa_instructions_offset > cfa_instructions_end) goto end;

    //build FDE info object
    if(NULL == (fde = malloc(sizeof(xcd_dwarf_fde_t)))) goto end;
    fde->cfa_instructions_offset = cfa_instructions_offset;
    fde->cfa_instructions_end = cfa_instructions_end;
    fde->pc_start = pc_start;
    fde->pc_end = pc_end;
    fde->cie = cie;

 end:
    *offset = (size_t)cfa_instructions_end; //pointer to next entry
    return fde;
}

static xcd_dwarf_fde_t *xcd_dwarf_get_fde_no_hdr(xcd_dwarf_t *self, uintptr_t pc)
{
    xcd_dwarf_fde_t *fde = NULL;
    size_t           offset = self->entries_offset;

    while(offset < self->entries_end)
    {
        if(NULL != (fde = xcd_dwarf_get_fde_from_offset(self, &offset, pc))) break;
    }
    return fde;
}

static int xcd_dwarf_get_fde_offset_from_pc(xcd_dwarf_t *self, uintptr_t pc, size_t *fde_offset)
{
    int r;
    uint64_t v64;
    int is_rel_encoded = ((self->eh_frame_hdr_table_encoding & 0x70) <= DW_EH_PE_funcrel ? 1 : 0);
    size_t first = 0;
    size_t last = self->eh_frame_hdr_fde_count;
    size_t cur;
    uintptr_t cur_pc;

    while(first < last)
    {
        cur = (first + last) / 2;

        //get current pc
        self->memory_cur_offset = self->entries_offset + cur * self->eh_frame_hdr_table_entry_size * 2;
        self->memory_pc_offset = 0;
        if(0 != (r = xcd_dwarf_read_encoded(self, &v64, self->eh_frame_hdr_table_encoding))) return r;
        cur_pc = (uintptr_t)v64;
        if(is_rel_encoded) cur_pc += self->load_bias;
        
        if(pc == cur_pc)
        {
            //get fde offset
            self->memory_pc_offset = 0;
            if(0 != (r = xcd_dwarf_read_encoded(self, &v64, self->eh_frame_hdr_table_encoding))) return r;
            *fde_offset = (size_t)v64;
            return 0;
        }
        else if(pc < cur_pc)
            last = cur;
        else
            first = cur + 1;
    }
    
    if(last != 0)
    {
        //get fde offset
        self->memory_cur_offset = self->entries_offset + (last - 1) * self->eh_frame_hdr_table_entry_size * 2 + self->eh_frame_hdr_table_entry_size;
        self->memory_pc_offset = 0;
        if(0 != (r = xcd_dwarf_read_encoded(self, &v64, self->eh_frame_hdr_table_encoding))) return r;
        *fde_offset = (size_t)v64;
        return 0;
    }
    
    return XCC_ERRNO_NOTFND;
}

static xcd_dwarf_fde_t *xcd_dwarf_get_fde_with_hdr(xcd_dwarf_t *self, uintptr_t pc)
{
    size_t offset;

    //get FDE-offset from PC
    if(0 != xcd_dwarf_get_fde_offset_from_pc(self, pc, &offset)) return NULL;

    //get FDE from FDE-offset
    return xcd_dwarf_get_fde_from_offset(self, &offset, pc);
}

static xcd_dwarf_fde_t *xcd_dwarf_get_fde(xcd_dwarf_t *self, uintptr_t pc)
{
    switch(self->type)
    {
    case XCD_DWARF_TYPE_DEBUG_FRAME:
    case XCD_DWARF_TYPE_EH_FRAME:
        return xcd_dwarf_get_fde_no_hdr(self, pc);
    case XCD_DWARF_TYPE_EH_FRAME_HDR:
        return xcd_dwarf_get_fde_with_hdr(self, pc);
    }
}

//////////////////////////////////////////////////////////////////////
// get LOC

static xcd_dwarf_loc_t *xcd_dwarf_get_loc(xcd_dwarf_t *self, xcd_dwarf_fde_t *fde, uintptr_t pc)
{
    xcd_dwarf_loc_t *loc = NULL, *loc_init = NULL, *loc_pc = NULL;
    
    size_t cie_instr_start = (size_t)fde->cie->cfa_instructions_offset;
    size_t cie_instr_end = (size_t)fde->cie->cfa_instructions_end;
    size_t fde_instr_start = (size_t)fde->cfa_instructions_offset;
    size_t fde_instr_end = (size_t)fde->cfa_instructions_end;
    
    uintptr_t cur_pc = fde->pc_start;
    
    uint8_t  v8, cfa_op, cfa_op_ext;
    uint64_t v64;
    size_t   i;

    xcd_dwarf_cfa_t *cfa;
    uintptr_t operands[2];

    xcd_dwarf_loc_node_t *loc_node, *loc_node_tmp;
    xcd_dwarf_loc_node_stack_t loc_node_stack = TAILQ_HEAD_INITIALIZER(loc_node_stack);

    //start from instructions in CIE
    self->memory_cur_offset = cie_instr_start;

    if(NULL == (loc_init = calloc(1, sizeof(xcd_dwarf_loc_t)))) return NULL;
    loc = loc_init;
    
    while(1)
    {
        if(cur_pc > pc) break; //have stepped to the LOC
        
        if(loc == loc_init)
        {
            if(self->memory_cur_offset >= cie_instr_end)
            {
                if(NULL == (loc_pc = calloc(1, sizeof(xcd_dwarf_loc_t)))) goto err;
                loc = loc_pc;

                //jump to FDE instructions
                self->memory_cur_offset = fde_instr_start;

                //save init instructions for DW_CFA_restore and DW_CFA_restore_extended
                memcpy(loc_pc, loc_init, sizeof(xcd_dwarf_loc_t));
            }
        }
        else
        {
            if(self->memory_cur_offset >= fde_instr_end)
            {
                break; //no more instructions
            }
        }
        
        if(0 != xcd_dwarf_read_bytes(self, &v8, 1)) goto err;
        cfa_op = v8 >> 6;
        cfa_op_ext = v8 & 0x3f;

        //opcode
        switch(cfa_op)
        {
        case 0x1: //DW_CFA_advance_loc
            cur_pc += cfa_op_ext * fde->cie->code_alignment_factor;
            break;
        case 0x2: //DW_CFA_offset
            if((uint16_t)cfa_op_ext >= XCD_DWARF_REG_NUM) goto err;
            if(0 != xcd_dwarf_read_uleb128(self, &v64)) goto err;
            loc->reg_rules[cfa_op_ext].type = DW_LOC_OFFSET;
            loc->reg_rules[cfa_op_ext].values[0] = (uint64_t)((int64_t)v64 * fde->cie->data_alignment_factor);
            break;
        case 0x3: //DW_CFA_restore
            if((uint16_t)cfa_op_ext >= XCD_DWARF_REG_NUM) goto err;
            loc->reg_rules[cfa_op_ext] = loc_init->reg_rules[cfa_op_ext];
            break;
        case 0:
        default:
            cfa = &(xcd_dwarf_cfa_table[cfa_op_ext]);
            if(1 == cfa->illegal) goto err;
            //read operands
            for(i = 0; i < 2; i++)
            {
                if(DW_EH_PE_omit == cfa->operand_types[i])
                    break;
                else if(DW_EH_PE_block == cfa->operand_types[i])
                {
                    if(0 != xcd_dwarf_read_uleb128(self, &v64)) goto err;
                    operands[i] = (uintptr_t)v64;
                    self->memory_cur_offset += (size_t)v64;
                }
                else
                {
                    if(0 != xcd_dwarf_read_encoded(self, &v64, cfa->operand_types[i])) goto err;
                    operands[i] = (uintptr_t)v64;
                }
            }
            //extended opcode
            switch(cfa_op_ext)
            {
            case 0x00: //DW_CFA_nop
                break;
            case 0x01: //DW_CFA_set_loc
                if(operands[0] < cur_pc) XCD_LOG_WARN("DWARF: PC is moving backwards");
                cur_pc = operands[0];
                break;
            case 0x02: //DW_CFA_advance_loc1
            case 0x03: //DW_CFA_advance_loc2
            case 0x04: //DW_CFA_advance_loc3
                cur_pc += operands[0] * fde->cie->code_alignment_factor;
                break;
            case 0x05: //DW_CFA_offset_extended
                if(operands[0] >= XCD_DWARF_REG_NUM) goto err;
                loc->reg_rules[operands[0]].type = DW_LOC_OFFSET;
                loc->reg_rules[operands[0]].values[0] = operands[1];
                break;
            case 0x06: //DW_CFA_restore_extended
                if(operands[0] >= XCD_DWARF_REG_NUM) goto err;
                loc->reg_rules[operands[0]] = loc_init->reg_rules[operands[0]];
                break;
            case 0x07: //DW_CFA_undefined
                if(operands[0] >= XCD_DWARF_REG_NUM) goto err;
                loc->reg_rules[operands[0]].type = DW_LOC_UNDEFINED;
                break;
            case 0x08: //DW_CFA_same_value
                if(operands[0] >= XCD_DWARF_REG_NUM) goto err;
                loc->reg_rules[operands[0]].type = DW_LOC_INVALID;
                break;
            case 0x09: //DW_CFA_register
                if(operands[0] >= XCD_DWARF_REG_NUM) goto err;
                loc->reg_rules[operands[0]].type = DW_LOC_REGISTER;
                loc->reg_rules[operands[0]].values[0] = operands[1];
                break;
            case 0x0a: //DW_CFA_remember_state
                if(NULL == (loc_node = malloc(sizeof(xcd_dwarf_loc_node_t)))) goto err;
                memcpy(&(loc_node->loc), loc, sizeof(xcd_dwarf_loc_t));
                TAILQ_INSERT_HEAD(&loc_node_stack, loc_node, link);
                break;
            case 0x0b: //DW_CFA_restore_state
                if(NULL == (loc_node = TAILQ_FIRST(&loc_node_stack)))
                {
                    XCD_LOG_WARN("DWARF: attempt to restore without remember");
                }
                else
                {
                    memcpy(loc, &(loc_node->loc), sizeof(xcd_dwarf_loc_t));
                    TAILQ_REMOVE(&loc_node_stack, loc_node, link);
                    free(loc_node);
                }
                break;
            case 0x0c: //DW_CFA_def_cfa
                loc->cfa_rule.type = DW_LOC_REGISTER;
                loc->cfa_rule.values[0] = operands[0];
                loc->cfa_rule.values[1] = operands[1];
                break;
            case 0x0d: //DW_CFA_def_cfa_register
                if(loc->cfa_rule.type != DW_LOC_REGISTER) goto err;
                loc->cfa_rule.values[0] = operands[0];
                break;
            case 0x0e: //DW_CFA_def_cfa_offset
                if(loc->cfa_rule.type != DW_LOC_REGISTER) goto err;
                loc->cfa_rule.values[1] = operands[0];
                break;
            case 0x0f: //DW_CFA_def_cfa_expression
                loc->cfa_rule.type = DW_LOC_VAL_EXPRESSION;
                loc->cfa_rule.values[0] = operands[0]; //expression length
                loc->cfa_rule.values[1] = self->memory_cur_offset; //expression end
                break;
            case 0x10: //DW_CFA_expression
                if(operands[0] >= XCD_DWARF_REG_NUM) goto err;
                loc->reg_rules[operands[0]].type = DW_LOC_EXPRESSION;
                loc->reg_rules[operands[0]].values[0] = operands[1]; //expression length
                loc->reg_rules[operands[0]].values[1] = self->memory_cur_offset; //expression end
                break;
            case 0x11: //DW_CFA_offset_extended_sf
                if(operands[0] >= XCD_DWARF_REG_NUM) goto err;
                loc->reg_rules[operands[0]].type = DW_LOC_OFFSET;
                loc->reg_rules[operands[0]].values[0] = (uint64_t)((int64_t)operands[1] * fde->cie->data_alignment_factor);
                break;
            case 0x12: //DW_CFA_def_cfa_sf
                loc->cfa_rule.type = DW_LOC_REGISTER;
                loc->cfa_rule.values[0] = operands[0];
                loc->cfa_rule.values[1] = (uint64_t)((int64_t)operands[1] * fde->cie->data_alignment_factor);
                break;
            case 0x13: //DW_CFA_def_cfa_offset_sf
                if(loc->cfa_rule.type != DW_LOC_REGISTER) goto err;
                loc->cfa_rule.values[1] = (uint64_t)((int64_t)operands[0] * fde->cie->data_alignment_factor);
                break;
            case 0x14: //DW_CFA_val_offset
            case 0x15: //DW_CFA_val_offset_sf
                if(operands[0] >= XCD_DWARF_REG_NUM) goto err;
                loc->reg_rules[operands[0]].type = DW_LOC_VAL_OFFSET;
                loc->reg_rules[operands[0]].values[0] = (uint64_t)((int64_t)operands[1] * fde->cie->data_alignment_factor);
                break;
            case 0x16: //DW_CFA_val_expression
                if(operands[0] >= XCD_DWARF_REG_NUM) goto err;
                loc->reg_rules[operands[0]].type = DW_LOC_VAL_EXPRESSION;
                loc->reg_rules[operands[0]].values[0] = operands[1]; //expression length
                loc->reg_rules[operands[0]].values[1] = self->memory_cur_offset; //expression end
                break;
            case 0x2e: //DW_CFA_GNU_args_size
                break;
            case 0x2f: //DW_CFA_GNU_negative_offset_extended
                if(operands[0] >= XCD_DWARF_REG_NUM) goto err;
                loc->reg_rules[operands[0]].type = DW_LOC_OFFSET;
                loc->reg_rules[operands[0]].values[0] = (uint64_t)(-((int64_t)operands[1]));
                break;
            default: // illegal
                goto err;
            }
            break;
        }
    }

 end:
    TAILQ_FOREACH_SAFE(loc_node, &loc_node_stack, link, loc_node_tmp)
    {
        TAILQ_REMOVE(&loc_node_stack, loc_node, link);
        free(loc_node);
    }
    if(loc == loc_pc && NULL != loc_init) free(loc_init);
    return loc;
    
 err:
    if(NULL != loc_init)
    {
        free(loc_init);
        loc_init = NULL;
    }
    if(NULL != loc_pc)
    {
        free(loc_pc);
        loc_pc = NULL;
    }
    loc = NULL;
    goto end;
}


//////////////////////////////////////////////////////////////////////
// eval

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wgnu-statement-expression"
static int xcd_dwarf_eval_expression(xcd_dwarf_t *self, xcd_regs_t *regs, size_t offset, size_t end, uintptr_t *result)
{
    uint8_t         op_code;
    xcd_dwarf_op_t *op;
    uintptr_t       operands[2];
    size_t          i, j = 0;
    int             r;
    uintptr_t       vup1, vup2, vup3;
    intptr_t        vip1;
    uint64_t        vu64;

    uintptr_t       stack[64]; //same as libunwind
    size_t          stack_idx = 0;

#define pop()   ({if(stack_idx - 1 >= 64) return XCC_ERRNO_FORMAT; stack[--stack_idx];})
#define push(x) do{if(stack_idx >= 64) return XCC_ERRNO_NOSPACE; stack[stack_idx++] = (x);}while(0)
#define pick(n) ({if(stack_idx - 1 - n >= 64) return XCC_ERRNO_FORMAT; stack[stack_idx - 1 - n];})

    self->memory_cur_offset = offset;

    //decode
    while(self->memory_cur_offset < end)
    {
        if(j++ > 1000) return XCC_ERRNO_RANGE;
            
        //read operation code
        if(0 != (r = xcd_dwarf_read_bytes(self, &op_code, 1))) return r;

        //get operation info
        op = &(xcd_dwarf_op_table[op_code]);

        //check illegal operation code
        if(1 == op->illegal) return XCC_ERRNO_ILLEGAL;

        //check required number of stack elements
        if(op->required_stack_values > stack_idx) return XCC_ERRNO_MISSING;

        //read operands
        for(i = 0; i < 2; i++)
        {
            if(DW_EH_PE_omit == op->operand_types[i])
                break;
            else
            {
                if(0 != (r = xcd_dwarf_read_encoded(self, &vu64, op->operand_types[i]))) return r;
                operands[i] = (uintptr_t)vu64;
            }
        }
        
        //do operation
        switch(op_code)
        {
        case 0x06: //DW_OP_deref
            if(0 != (r = xcd_util_ptrace_read_fully(self->pid, pop(), &vup1, sizeof(vup1)))) return r;
            push(vup1);
            break;
        case 0x94: //DW_OP_deref_size
            if(0 == operands[0] || operands[0] > sizeof(uintptr_t)) return XCC_ERRNO_FORMAT;
            if(0 != (r = xcd_util_ptrace_read_fully(self->pid, pop(), &vup1, operands[0]))) return r;
            push(vup1);
            break;
        case 0x03: //DW_OP_addr
        case 0x08: //DW_OP_const1u
        case 0x09: //DW_OP_const1s
        case 0x0a: //DW_OP_const2u
        case 0x0b: //DW_OP_const2s
        case 0x0c: //DW_OP_const4u
        case 0x0d: //DW_OP_const4s
        case 0x0e: //DW_OP_const8u
        case 0x0f: //DW_OP_const8s
        case 0x10: //DW_OP_constu
        case 0x11: //DW_OP_consts
            push(operands[0]);
            break;
        case 0x12: //DW_OP_dup
            push(pick(0));
            break;
        case 0x13: //DW_OP_drop
            pop();
            break;
        case 0x14: //DW_OP_over
            push(pick(1));
            break;
        case 0x15: //DW_OP_pick:
            if(operands[0] > stack_idx) return XCC_ERRNO_FORMAT;
            push(pick(operands[0]));
            break;
        case 0x16: //DW_OP_swap
            vup1 = pop();
            vup2 = pop();
            push(vup1);
            push(vup2);
            break;
        case 0x17: //DW_OP_rot
            vup1 = pop();
            vup2 = pop();
            vup3 = pop();
            push(vup1);
            push(vup3);
            push(vup2);
            break;
        case 0x19: //DW_OP_abs
            vip1 = (intptr_t)pop();
            if(vip1 < 0) vip1 = -vip1;
            push((uintptr_t)vip1);
            break;
        case 0x1a: //DW_OP_and
            vup1 = pop();
            vup2 = pop();
            push(vup1 & vup2);
            break;
        case 0x1b: //DW_OP_div
            vup1 = pop();
            vup2 = pop();
            if(0 == vup1) return XCC_ERRNO_FORMAT;
            push((uintptr_t)((intptr_t)vup2 / (intptr_t)vup1));
            break;
        case 0x1c: //DW_OP_minus
            vup1 = pop();
            vup2 = pop();
            push(vup2 - vup1);
            break;
        case 0x1d: //DW_OP_mod
            vup1 = pop();
            vup2 = pop();
            if(0 == vup1) return XCC_ERRNO_FORMAT;
            push(vup2 % vup1);
            break;
        case 0x1e: //DW_OP_mul
            vup1 = pop();
            vup2 = pop();
            push(vup1 * vup2);
            break;
        case 0x1f: //DW_OP_neg
            push((uintptr_t)(-((intptr_t)pop())));
            break;
        case 0x20: //DW_OP_not
            push(~pop());
            break;
        case 0x21: //DW_OP_or
            vup1 = pop();
            vup2 = pop();
            push(vup1 | vup2);
            break;
        case 0x22: //DW_OP_plus
            vup1 = pop();
            vup2 = pop();
            push(vup1 + vup2);
            break;
        case 0x23: //DW_OP_plus_uconst
            push(pop() + operands[0]);
            break;
        case 0x24: //DW_OP_shl
            vup1 = pop();
            vup2 = pop();
            push(vup2 << vup1);
            break;
        case 0x25: //DW_OP_shr
            vup1 = pop();
            vup2 = pop();
            push(vup2 >> vup1);
            break;
        case 0x26: //DW_OP_shra
            vup1 = pop();
            vup2 = pop();
            push((uintptr_t)((intptr_t)vup2 >> vup1));
            break;
        case 0x27: //DW_OP_xor
            vup1 = pop();
            vup2 = pop();
            push(vup1 ^ vup2);
            break;
        case 0x28: //DW_OP_bra
            vup1 = pop();
            if(0 != vup1)
                self->memory_cur_offset = (size_t)((ssize_t)self->memory_cur_offset + (ssize_t)operands[0]);
            else
                self->memory_cur_offset = (size_t)((ssize_t)self->memory_cur_offset - (ssize_t)operands[0]);
            break;
        case 0x29: //DW_OP_eq
            vup1 = pop();
            vup2 = pop();
            push((intptr_t)vup1 == (intptr_t)vup2 ? 1 : 0);
            break;
        case 0x2a: //DW_OP_ge
            vup1 = pop();
            vup2 = pop();
            push((intptr_t)vup2 >= (intptr_t)vup1 ? 1 : 0);
            break;
        case 0x2b: //DW_OP_gt
            vup1 = pop();
            vup2 = pop();
            push((intptr_t)vup2 > (intptr_t)vup1 ? 1 : 0);
            break;
        case 0x2c: //DW_OP_le
            vup1 = pop();
            vup2 = pop();
            push((intptr_t)vup2 <= (intptr_t)vup1 ? 1 : 0);
            break;
        case 0x2d: //DW_OP_lt
            vup1 = pop();
            vup2 = pop();
            push((intptr_t)vup2 < (intptr_t)vup1 ? 1 : 0);
            break;
        case 0x2e: //DW_OP_ne
            vup1 = pop();
            vup2 = pop();
            push((intptr_t)vup1 != (intptr_t)vup2 ? 1 : 0);
            break;
        case 0x2f: //DW_OP_skip
            self->memory_cur_offset = (size_t)((ssize_t)self->memory_cur_offset + (ssize_t)operands[0]);
            break;
        case 0x30: //DW_OP_lit0
        case 0x31: //DW_OP_lit1
        case 0x32: //DW_OP_lit2
        case 0x33: //DW_OP_lit3
        case 0x34: //DW_OP_lit4
        case 0x35: //DW_OP_lit5
        case 0x36: //DW_OP_lit6
        case 0x37: //DW_OP_lit7
        case 0x38: //DW_OP_lit8
        case 0x39: //DW_OP_lit9
        case 0x3a: //DW_OP_lit10
        case 0x3b: //DW_OP_lit11
        case 0x3c: //DW_OP_lit12
        case 0x3d: //DW_OP_lit13
        case 0x3e: //DW_OP_lit14
        case 0x3f: //DW_OP_lit15
        case 0x40: //DW_OP_lit16
        case 0x41: //DW_OP_lit17
        case 0x42: //DW_OP_lit18
        case 0x43: //DW_OP_lit19
        case 0x44: //DW_OP_lit20
        case 0x45: //DW_OP_lit21
        case 0x46: //DW_OP_lit22
        case 0x47: //DW_OP_lit23
        case 0x48: //DW_OP_lit24
        case 0x49: //DW_OP_lit25
        case 0x4a: //DW_OP_lit26
        case 0x4b: //DW_OP_lit27
        case 0x4c: //DW_OP_lit28
        case 0x4d: //DW_OP_lit29
        case 0x4e: //DW_OP_lit30
        case 0x4f: //DW_OP_lit31
            push(op_code - 0x30);
            break;
        case 0x70: //DW_OP_breg0
        case 0x71: //DW_OP_breg1
        case 0x72: //DW_OP_breg2
        case 0x73: //DW_OP_breg3
        case 0x74: //DW_OP_breg4
        case 0x75: //DW_OP_breg5
        case 0x76: //DW_OP_breg6
        case 0x77: //DW_OP_breg7
        case 0x78: //DW_OP_breg8
        case 0x79: //DW_OP_breg9
        case 0x7a: //DW_OP_breg10
        case 0x7b: //DW_OP_breg11
        case 0x7c: //DW_OP_breg12
        case 0x7d: //DW_OP_breg13
        case 0x7e: //DW_OP_breg14
        case 0x7f: //DW_OP_breg15
        case 0x80: //DW_OP_breg16
        case 0x81: //DW_OP_breg17
        case 0x82: //DW_OP_breg18
        case 0x83: //DW_OP_breg19
        case 0x84: //DW_OP_breg20
        case 0x85: //DW_OP_breg21
        case 0x86: //DW_OP_breg22
        case 0x87: //DW_OP_breg23
        case 0x88: //DW_OP_breg24
        case 0x89: //DW_OP_breg25
        case 0x8a: //DW_OP_breg26
        case 0x8b: //DW_OP_breg27
        case 0x8c: //DW_OP_breg28
        case 0x8d: //DW_OP_breg29
        case 0x8e: //DW_OP_breg30
        case 0x8f: //DW_OP_breg31
            if(op_code - 0x70 >= XCD_REGS_MACHINE_NUM) return XCC_ERRNO_FORMAT;
            push(regs->r[op_code - 0x70] + operands[0]);
            break;
        case 0x92: //DW_OP_bregx
            if(operands[0] >= XCD_REGS_MACHINE_NUM) return XCC_ERRNO_FORMAT;
            push(regs->r[operands[0]] + operands[1]);
            break;
        case 0x96: //DW_OP_nop
            break;
        default:
            return XCC_ERRNO_ILLEGAL;
        }
    }

    if(0 == stack_idx) return XCC_ERRNO_FORMAT;
    *result = pick(0);
    return 0;
}
#pragma clang diagnostic pop
    
static int xcd_dwarf_eval(xcd_dwarf_t *self, xcd_dwarf_fde_t *fde, xcd_dwarf_loc_t *loc, xcd_regs_t *regs, int *finished)
{
    int        r;
    size_t     i;
    uintptr_t  cfa;
    uintptr_t  value;
    xcd_regs_t regs_orig = *regs;
    int        return_address_undefined = 0;

    //CFA
    switch(loc->cfa_rule.type)
    {
    case DW_LOC_REGISTER:
        if(loc->cfa_rule.values[0] >= XCD_REGS_MACHINE_NUM) return XCC_ERRNO_RANGE;
        cfa = (uintptr_t)(regs_orig.r[loc->cfa_rule.values[0]] + loc->cfa_rule.values[1]);
        break;
    case DW_LOC_VAL_EXPRESSION:
        if(0 != (r = xcd_dwarf_eval_expression(self, &regs_orig, (size_t)(loc->cfa_rule.values[1] - loc->cfa_rule.values[0]),
                                               (size_t)(loc->cfa_rule.values[1]), &value))) return r;
        cfa = value;
        break;
    default:
        return XCC_ERRNO_ILLEGAL;
    }

    //regs
    for(i = 0; i < XCD_REGS_MACHINE_NUM; i++)
    {
        switch(loc->reg_rules[i].type)
        {
        case DW_LOC_OFFSET:
            if(0 != (r = xcd_util_ptrace_read_fully(self->pid, (uintptr_t)(cfa + loc->reg_rules[i].values[0]), &(regs->r[i]), sizeof(regs->r[i])))) return r;
            break;
        case DW_LOC_VAL_OFFSET:
            regs->r[i] = (uintptr_t)(cfa + loc->reg_rules[i].values[0]);
            break;
        case DW_LOC_REGISTER:
            if(loc->reg_rules[i].values[0] >= XCD_REGS_MACHINE_NUM) return XCC_ERRNO_RANGE;
            regs->r[i] = (uintptr_t)(regs_orig.r[loc->reg_rules[i].values[0]] + loc->reg_rules[i].values[1]);
            break;
        case DW_LOC_EXPRESSION:
            if(0 != (r = xcd_dwarf_eval_expression(self, &regs_orig, (size_t)(loc->reg_rules[i].values[1] - loc->reg_rules[i].values[0]),
                                                   (size_t)(loc->reg_rules[i].values[1]), &value))) return r;
            if(0 != (r = xcd_util_ptrace_read_fully(self->pid, value, &(regs->r[i]), sizeof(regs->r[i])))) return r;
            break;
        case DW_LOC_VAL_EXPRESSION:
            if(0 != (r = xcd_dwarf_eval_expression(self, &regs_orig, (size_t)(loc->reg_rules[i].values[1] - loc->reg_rules[i].values[0]),
                                                   (size_t)(loc->reg_rules[i].values[1]), &value))) return r;
            regs->r[i] = value;
            break;
        case DW_LOC_UNDEFINED:
            if(i == fde->cie->return_address_register)
                return_address_undefined = 1;
            break;
        default:
            break;
        }
    }

    //step PC and SP
    xcd_regs_set_pc(regs, return_address_undefined ? 0 : regs->r[fde->cie->return_address_register]);
    xcd_regs_set_sp(regs, cfa);
    
    //if the pc was set to zero, consider this the final frame
    *finished = (0 == xcd_regs_get_pc(regs) ? 1 : 0);

    return 0;
}


//////////////////////////////////////////////////////////////////////
// create DWARF object

static int xcd_dwarf_init_eh_frame_hdr(xcd_dwarf_t *self)
{
    int      r;
    uint64_t v64;
    uint8_t  data[4];
    uint8_t  version;
    uint8_t  ptr_encoding;
    uint8_t  fde_count_encoding;
    uint8_t  table_encoding;

    if(0 != (r = xcd_dwarf_read_bytes(self, data, 4))) return r;
    version = data[0];
    ptr_encoding = data[1];
    fde_count_encoding = data[2];
    table_encoding = data[3];

    //check version
    if(1 != version) return XCC_ERRNO_FORMAT;

    //save table encoding
    self->eh_frame_hdr_table_encoding = table_encoding;

    //save table entry size
    switch(table_encoding & 0x0f)
    {
    case DW_EH_PE_absptr:
        self->eh_frame_hdr_table_entry_size = sizeof(uintptr_t);
        break;
    case DW_EH_PE_udata1:
    case DW_EH_PE_sdata1:
        self->eh_frame_hdr_table_entry_size = 1;
        break;
    case DW_EH_PE_udata2:
    case DW_EH_PE_sdata2:
        self->eh_frame_hdr_table_entry_size = 2;
        break;
    case DW_EH_PE_udata4:
    case DW_EH_PE_sdata4:
        self->eh_frame_hdr_table_entry_size = 4;
        break;
    case DW_EH_PE_udata8:
    case DW_EH_PE_sdata8:
        self->eh_frame_hdr_table_entry_size = 8;
        break;
    case DW_EH_PE_uleb128:
    case DW_EH_PE_sleb128:
    default:
        return XCC_ERRNO_FORMAT; //using .eh_frame for linear search
    }
    
    //skip .eh_frame ptr
    self->memory_pc_offset = self->memory_cur_offset;
    if(0 != (r = xcd_dwarf_read_encoded(self, &v64, ptr_encoding))) return r;

    //get fde count
    self->memory_pc_offset = self->memory_cur_offset;
    if(0 != (r = xcd_dwarf_read_encoded(self, &v64, fde_count_encoding))) return r;
    if(0 == v64) return XCC_ERRNO_FORMAT;
    self->eh_frame_hdr_fde_count = (size_t)v64;

    //set entries_offset to the start of binary search table
    self->entries_offset = self->memory_cur_offset;
    
    return 0;
}

int xcd_dwarf_create(xcd_dwarf_t **self, xcd_memory_t *memory, pid_t pid, uintptr_t load_bias,
                     size_t offset, size_t size, xcd_dwarf_type_t type)
{
    int r = 0;
    
    if(NULL == (*self = calloc(1, sizeof(xcd_dwarf_t)))) return XCC_ERRNO_NOMEM;
    (*self)->type = type;
    (*self)->pid = pid;
    (*self)->load_bias = load_bias;
    RB_INIT(&((*self)->cie_cache));
    (*self)->memory = memory;
    (*self)->memory_cur_offset = offset;
    (*self)->memory_pc_offset = (size_t)-1;
    (*self)->memory_data_offset = offset;
    (*self)->pc_offset = offset;
    (*self)->entries_offset = offset;
    (*self)->entries_end = offset + size;

    //for .eh_frame_hdr
    if(XCD_DWARF_TYPE_EH_FRAME_HDR == type)
        if(0 != (r = xcd_dwarf_init_eh_frame_hdr(*self))) goto err;

    return 0;

 err:
    if(NULL != *self)
    {
        free(*self);
        *self = NULL;
    }
    return r;
}

//////////////////////////////////////////////////////////////////////
// get step

int xcd_dwarf_step(xcd_dwarf_t *self, xcd_regs_t *regs, uintptr_t pc, int *finished)
{
    xcd_dwarf_fde_t *fde = NULL;
    xcd_dwarf_loc_t *loc = NULL;
    int              r   = XCC_ERRNO_NOTFND;

    //find FDE & CIE from PC
    if(NULL == (fde = xcd_dwarf_get_fde(self, pc)))
    {
#if XCD_DWARF_DEBUG
        XCD_LOG_DEBUG("DWARF: get FDE failed, step_pc=%"PRIxPTR, pc);
#endif
        goto end;
    }
    
    //find LOCATION in the FDE from PC
    if(NULL == (loc = xcd_dwarf_get_loc(self, fde, pc)))
    {
#if XCD_DWARF_DEBUG
        XCD_LOG_DEBUG("DWARF: get LOC failed, step_pc=%"PRIxPTR, pc);
#endif
        goto end;
    }

    //eval the actual registers
    if(0 != (r = xcd_dwarf_eval(self, fde, loc, regs, finished)))
    {
#if XCD_DWARF_DEBUG
        XCD_LOG_DEBUG("DWARF: eval failed, step_pc=%"PRIxPTR, pc);
#endif
        goto end;
    }

    //OK
    r = 0;

 end:
    if(NULL != fde) free(fde);
    if(NULL != loc) free(loc);
    return r;
}

#pragma clang diagnostic pop
