/*
 * SPDX-FileCopyrightText: 2021-2026 LATX Project Authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#ifndef __SEGMENT_H_
#define __SEGMENT_H_

#include "qemu-def.h"

typedef struct seg_info {
    char *file_name;
    target_ulong file_offset;
    uint64_t seg_begin;
    uint64_t seg_end;
    uint64_t tv_sec;
    uint64_t lib_size;

    union {
        uint64_t first_tb_id;
        void *buffer;
    };
    union {
        uint64_t last_tb_id;
        void *p_segment;
    };
#define SEG_RUNNING     0x1
#define IS_ELF_SEG      0x2
    uint8_t seg_flag;
} seg_info;

typedef struct wine_sec_info {
    target_ulong addr;
    uint64_t offset;
    char *file_name;
} wine_sec_info;
typedef struct wine_dos_mz_head {
    int16_t e_magic;
    int8_t nop[58];
    int32_t e_lfanew;
} wine_dos_mz_head;

typedef struct wine_pe_nt_head {
    int8_t nop[6];
    int16_t NumberOfSections;
    int8_t nop1[12];
    int16_t sec_pos;
    int8_t nop2[2];
    int16_t Magic;
} __attribute__((packed)) wine_pe_nt_head;

typedef struct wine_sec_head {
    char Name[8];
    int32_t VirtualSize;
    int32_t VirtualAddress;
    int32_t SizeOfRawData;
    int8_t nop[20];
} __attribute__((packed)) wine_sec_head;

#define WINE_DOS_H_MZ_SIZE 96
#define WINE_PE_H_NT_SIZE 264
#define WINE_PE_H_SEC_POS 24
#define WINE_PE_H_SEC_MAX_NUMS 96

#define WINE_HEAD_MIN_SIZE (WINE_DOS_H_MZ_SIZE + \
    WINE_PE_H_NT_SIZE + sizeof(wine_sec_head))

#define IMAGE_DOS_SIGNATURE    0x5A4D     /* MZ   */
#define IMAGE_NT_OPTIONAL_HDR32_MAGIC      0x10b
#define IMAGE_NT_OPTIONAL_HDR64_MAGIC      0x20b

bool wine_dll_handle(char *file_name, int name_len, int target_prot,
    abi_ulong map_start, abi_ulong map_len, int map_offset, int map_fd);
void wine_sec_tree_init(void);
wine_sec_info *wine_sec_tree_lookup(target_ulong pc);
void segment_tree_init(void);
uint64_t deal_seg(wine_sec_info *wine_sec, bool is_pread, uint64_t aot_offset,
        char *buf, int fd, int target_prot, abi_long len, abi_long start);
void segment_tree_insert(char *name, target_ulong offset, target_ulong begin,
        target_ulong end);
seg_info *segment_tree_lookup(target_ulong pc);
seg_info *segment_tree_lookup2(target_ulong begin, target_ulong end);
void segment_tree_remove(seg_info *val);
bool segment_tree_winepe_lookup(target_ulong pc);
gint get_segment_num(void);
void do_segment_record(seg_info **seg_info_vector);

#endif
