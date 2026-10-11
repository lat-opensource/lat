#include "qemu/osdep.h"

#include "latc_native_export.h"
#include "lat-aot-v2.h"
#include "lat-native-image.h"
#include "lat-eflags-link.h"
#include "latc-build-id.h"
#include "latc-bundle-loader.h"
#include "crypto/aes.h"
#include "../translator/tr-vpaes.h"

#include <elf.h>
#include <glib.h>

static uint64_t align_up(uint64_t value, uint64_t alignment)
{
    return (value + alignment - 1) & ~(alignment - 1);
}

static uint32_t native_semantic_flags(uint32_t cflags)
{
    return LAT_AOT_TB_CODE64 |
        ((cflags & CF_PARALLEL) ? LAT_AOT_TB_PARALLEL : 0);
}

static gint compare_native_tb(gconstpointer left, gconstpointer right)
{
    const LatNativeTbV1 *a = left;
    const LatNativeTbV1 *b = right;
    if (a->guest_pc != b->guest_pc) {
        return a->guest_pc < b->guest_pc ? -1 : 1;
    }
    if (a->flags != b->flags) {
        return a->flags < b->flags ? -1 : 1;
    }
    return 0;
}

static void sort_native_tbs(GArray *native_tbs)
{
    g_array_sort(native_tbs, compare_native_tb);
}

static gint compare_native_tb_code(gconstpointer left, gconstpointer right)
{
    const LatNativeTbV1 *a = *(const LatNativeTbV1 *const *)left;
    const LatNativeTbV1 *b = *(const LatNativeTbV1 *const *)right;
    if (a->code_offset != b->code_offset) {
        return a->code_offset < b->code_offset ? -1 : 1;
    }
    return 0;
}

static int native_tb_index_for_code(const GArray *native_tbs,
                                    const GArray *code_order,
                                    uint64_t code_offset)
{
    guint left = 0;
    guint right = code_order->len;
    while (left < right) {
        guint middle = left + (right - left) / 2;
        const LatNativeTbV1 *tb = g_array_index(
            code_order, const LatNativeTbV1 *, middle);
        if (tb->code_offset <= code_offset) {
            left = middle + 1;
        } else {
            right = middle;
        }
    }
    if (!left) {
        return -1;
    }
    const LatNativeTbV1 *tb = g_array_index(
        code_order, const LatNativeTbV1 *, left - 1);
    if (code_offset >= tb->code_offset + tb->code_size) {
        return -1;
    }
    return (int)(tb - &g_array_index(native_tbs, LatNativeTbV1, 0));
}

static gint compare_uint64(gconstpointer left, gconstpointer right)
{
    uint64_t a = *(const uint64_t *)left;
    uint64_t b = *(const uint64_t *)right;
    return a < b ? -1 : a > b;
}

static gint compare_native_pc_map(gconstpointer left, gconstpointer right)
{
    const LatNativePcMapV2 *a = left;
    const LatNativePcMapV2 *b = right;
    if (a->host_offset_begin != b->host_offset_begin) {
        return a->host_offset_begin < b->host_offset_begin ? -1 : 1;
    }
    if (a->host_offset_end != b->host_offset_end) {
        return a->host_offset_end < b->host_offset_end ? -1 : 1;
    }
    return 0;
}

static bool native_tb_target_exists(const GArray *tbs, uint64_t guest_pc,
                                    uint32_t flags)
{
    guint left = 0;
    guint right = tbs->len;
    while (left < right) {
        guint middle = left + (right - left) / 2;
        const LatNativeTbV1 *tb = &g_array_index(
            tbs, LatNativeTbV1, middle);
        if (tb->guest_pc < guest_pc ||
            (tb->guest_pc == guest_pc && tb->flags < flags)) {
            left = middle + 1;
        } else {
            right = middle;
        }
    }
    if (left < tbs->len) {
        const LatNativeTbV1 *tb = &g_array_index(
            tbs, LatNativeTbV1, left);
        if (tb->guest_pc == guest_pc && tb->flags == flags) {
            return true;
        }
    }

    left = 0;
    right = tbs->len;
    while (left < right) {
        guint middle = left + (right - left) / 2;
        const LatNativeTbV1 *tb = &g_array_index(
            tbs, LatNativeTbV1, middle);
        if (tb->guest_pc < guest_pc) {
            left = middle + 1;
        } else {
            right = middle;
        }
    }
    if (left >= tbs->len) {
        return false;
    }
    const LatNativeTbV1 *tb = &g_array_index(tbs, LatNativeTbV1, left);
    if (tb->guest_pc != guest_pc) {
        return false;
    }
    return left + 1 == tbs->len ||
        g_array_index(tbs, LatNativeTbV1, left + 1).guest_pc != guest_pc;
}

static bool guest_executable_address(const GByteArray *guest, uint64_t pc,
                                     uint64_t load_bias)
{
    if (pc < load_bias) {
        return false;
    }
    pc -= load_bias;
    const Elf64_Ehdr *elf = (const void *)guest->data;
    const Elf64_Phdr *program_headers =
        (const void *)(guest->data + elf->e_phoff);
    for (uint16_t i = 0; i < elf->e_phnum; i++) {
        const Elf64_Phdr *header = &program_headers[i];
        if (header->p_type == PT_LOAD && (header->p_flags & PF_X) &&
            pc >= header->p_vaddr && pc - header->p_vaddr < header->p_memsz) {
            return true;
        }
    }
    return false;
}

static int runtime_symbol(aot_rel_kind kind)
{
    switch (kind) {
    case B_EPILOGUE_RET_ID_1: return LAT_NATIVE_SYMBOL_EPILOGUE_RET_ID_1;
    case B_EPILOGUE_RET_ID_0: return LAT_NATIVE_SYMBOL_EPILOGUE_RET_ID_0;
    case JIRL_EPILOGUE_RET_ID_1:
        return LAT_NATIVE_SYMBOL_JIRL_EPILOGUE_RET_ID_1;
    case JIRL_EPILOGUE_RET_ID_0:
        return LAT_NATIVE_SYMBOL_JIRL_EPILOGUE_RET_ID_0;
    case B_EPILOGUE_RET_0: return LAT_NATIVE_SYMBOL_EPILOGUE_RET_0;
    case LOAD_HELPER_UPDATE_MXCSR_STATUS:
        return LAT_NATIVE_SYMBOL_UPDATE_MXCSR_STATUS;
    case LOAD_HELPER_FXSAVE: return LAT_NATIVE_SYMBOL_FXSAVE;
    case LOAD_HELPER_FXRSTOR: return LAT_NATIVE_SYMBOL_FXRSTOR;
    case LOAD_HELPER_CONVERT_FPREGS_X80_TO_64:
        return LAT_NATIVE_SYMBOL_FPREGS_X80_TO_64;
    case LOAD_HELPER_CONVERT_FPREGS_64_TO_X80:
        return LAT_NATIVE_SYMBOL_FPREGS_64_TO_X80;
    case LOAD_HELPER_UPDATE_FP_STATUS:
        return LAT_NATIVE_SYMBOL_UPDATE_FP_STATUS;
    case LOAD_HELPER_CPUID: return LAT_NATIVE_SYMBOL_CPUID;
    case LOAD_HELPER_RAISE_ILLOP: return LAT_NATIVE_SYMBOL_RAISE_ILLOP;
    case LOAD_HELPER_RAISE_GPF: return LAT_NATIVE_SYMBOL_RAISE_GPF;
    case LOAD_HELPER_RAISE_SYSCALL: return LAT_NATIVE_SYMBOL_RAISE_SYSCALL;
    case LOAD_HOST_PFTABLE: return LAT_NATIVE_SYMBOL_PFTABLE;
    case LOAD_HELPER_PCMPISTRI_XMM:
        return LAT_NATIVE_SYMBOL_PCMPISTRI_XMM;
    case LOAD_HELPER_PCMPISTRM_XMM:
        return LAT_NATIVE_SYMBOL_PCMPISTRM_XMM;
    case LOAD_HELPER_EFLAGTF: return LAT_NATIVE_SYMBOL_EFLAGTF;
    case LOAD_HOST_LOG2: return LAT_NATIVE_SYMBOL_LOG2;
    case LOAD_HOST_POW: return LAT_NATIVE_SYMBOL_POW;
    case LOAD_HOST_SIN: return LAT_NATIVE_SYMBOL_SIN;
    case LOAD_HOST_COS: return LAT_NATIVE_SYMBOL_COS;
    case LOAD_HOST_ATAN2: return LAT_NATIVE_SYMBOL_ATAN2;
    case LOAD_HOST_LOGB: return LAT_NATIVE_SYMBOL_LOGB;
    case LOAD_HOST_SINCOS: return LAT_NATIVE_SYMBOL_SINCOS;
    case LOAD_HELPER_FPATAN: return LAT_NATIVE_SYMBOL_FPATAN;
    case LOAD_HELPER_FPTAN: return LAT_NATIVE_SYMBOL_FPTAN;
    case LOAD_HELPER_FPREM: return LAT_NATIVE_SYMBOL_FPREM;
    case LOAD_HELPER_FPREM1: return LAT_NATIVE_SYMBOL_FPREM1;
    case LOAD_HELPER_FRNDINT: return LAT_NATIVE_SYMBOL_FRNDINT;
    case LOAD_HELPER_F2XM1: return LAT_NATIVE_SYMBOL_F2XM1;
    case LOAD_HELPER_FXTRACT: return LAT_NATIVE_SYMBOL_FXTRACT;
    case LOAD_HELPER_FYL2X: return LAT_NATIVE_SYMBOL_FYL2X;
    case LOAD_HELPER_FYL2XP1: return LAT_NATIVE_SYMBOL_FYL2XP1;
    case LOAD_HELPER_FSINCOS: return LAT_NATIVE_SYMBOL_FSINCOS;
    case LOAD_HELPER_FSIN: return LAT_NATIVE_SYMBOL_FSIN;
    case LOAD_HELPER_FCOS: return LAT_NATIVE_SYMBOL_FCOS;
    case LOAD_HELPER_FBLD_ST0: return LAT_NATIVE_SYMBOL_FBLD_ST0;
    case LOAD_HELPER_FBST_ST0: return LAT_NATIVE_SYMBOL_FBST_ST0;
    case LOAD_HELPER_AESIMC_XMM: return LAT_NATIVE_SYMBOL_AESIMC_XMM;
    case LOAD_HELPER_AESKEYGENASSIST_XMM:
        return LAT_NATIVE_SYMBOL_AESKEYGENASSIST_XMM;
    case LOAD_HELPER_AESDEC_XMM: return LAT_NATIVE_SYMBOL_AESDEC_XMM;
    case LOAD_HELPER_AESDECLAST_XMM:
        return LAT_NATIVE_SYMBOL_AESDECLAST_XMM;
    case LOAD_HELPER_SHA1NEXTE: return LAT_NATIVE_SYMBOL_SHA1NEXTE;
    case LOAD_HELPER_SHA1MSG1: return LAT_NATIVE_SYMBOL_SHA1MSG1;
    case LOAD_HELPER_SHA1MSG2: return LAT_NATIVE_SYMBOL_SHA1MSG2;
    case LOAD_HELPER_SHA256MSG1: return LAT_NATIVE_SYMBOL_SHA256MSG1;
    case LOAD_HELPER_SHA256MSG2: return LAT_NATIVE_SYMBOL_SHA256MSG2;
    case LOAD_HELPER_SHA1RNDS4_F0: return LAT_NATIVE_SYMBOL_SHA1RNDS4_F0;
    case LOAD_HELPER_SHA1RNDS4_F1: return LAT_NATIVE_SYMBOL_SHA1RNDS4_F1;
    case LOAD_HELPER_SHA1RNDS4_F2: return LAT_NATIVE_SYMBOL_SHA1RNDS4_F2;
    case LOAD_HELPER_SHA1RNDS4_F3: return LAT_NATIVE_SYMBOL_SHA1RNDS4_F3;
    case LOAD_HELPER_SHA256RNDS2_XMM0:
        return LAT_NATIVE_SYMBOL_SHA256RNDS2_XMM0;
    case LOAD_HELPER_RAISE_INT: return LAT_NATIVE_SYMBOL_RAISE_INT;
    case LOAD_HELPER_RAISE_TRAPOP: return LAT_NATIVE_SYMBOL_RAISE_TRAPOP;
    case LOAD_HELPER_RAISE_INTO: return LAT_NATIVE_SYMBOL_RAISE_INTO;
    case LOAD_HELPER_RAISE_BOUND: return LAT_NATIVE_SYMBOL_RAISE_BOUND;
    case LOAD_HELPER_XGETBV: return LAT_NATIVE_SYMBOL_XGETBV;
    case LOAD_HELPER_XSAVE: return LAT_NATIVE_SYMBOL_XSAVE;
    case LOAD_HELPER_XSAVEOPT: return LAT_NATIVE_SYMBOL_XSAVEOPT;
    case LOAD_HELPER_XRSTOR: return LAT_NATIVE_SYMBOL_XRSTOR;
    case LOAD_HELPER_XSETBV: return LAT_NATIVE_SYMBOL_XSETBV;
    case LOAD_HELPER_KZT_GET_ALTERNATE:
        return LAT_NATIVE_SYMBOL_KZT_GET_ALTERNATE;
    default: return LAT_NATIVE_SYMBOL_INVALID;
    }
}

static const aot_segment *find_tb_segment(const aot_header *header,
        const aot_segment *segments, const aot_tb *tb)
{
    uintptr_t address = (uintptr_t)tb;
    for (uint32_t i = 0; i < header->segments_num; i++) {
        uintptr_t first = (uintptr_t)header + segments[i].segment_tbs_offset;
        uintptr_t end = first +
            (uint64_t)segments[i].segment_tbs_num * sizeof(*tb);
        if (address >= first && address < end) {
            return &segments[i];
        }
    }
    return NULL;
}

static int copy_guest(const char *path, GByteArray **guest,
        uint64_t *entry, uint64_t *base, uint8_t digest[32],
        bool digest_verified)
{
    gchar *data = NULL;
    gsize size = 0;
    GError *error = NULL;
    if (!g_file_get_contents(path, &data, &size, &error)) {
        fprintf(stderr, "latc: cannot read native guest %s: %s\n", path,
                error ? error->message : "unknown error");
        g_clear_error(&error);
        return -1;
    }
    if (size < sizeof(Elf64_Ehdr)) {
        fprintf(stderr, "latc: native guest is not ELF64\n");
        g_free(data);
        return -1;
    }
    const Elf64_Ehdr *elf = (const void *)data;
    if (memcmp(elf->e_ident, ELFMAG, SELFMAG) ||
        elf->e_ident[EI_CLASS] != ELFCLASS64 ||
        elf->e_machine != EM_X86_64) {
        fprintf(stderr, "latc: native guest is not an x86-64 ELF\n");
        g_free(data);
        return -1;
    }
    if (elf->e_phentsize != sizeof(Elf64_Phdr) ||
        elf->e_phoff > size ||
        (uint64_t)elf->e_phnum * sizeof(Elf64_Phdr) > size - elf->e_phoff) {
        fprintf(stderr, "latc: native guest has an invalid program header table\n");
        g_free(data);
        return -1;
    }
    const Elf64_Phdr *program_headers =
        (const void *)((const uint8_t *)data + elf->e_phoff);
    *base = UINT64_MAX;
    for (uint16_t i = 0; i < elf->e_phnum; i++) {
        if (program_headers[i].p_type == PT_LOAD &&
            program_headers[i].p_vaddr < *base) {
            *base = program_headers[i].p_vaddr;
        }
    }
    if (*base == UINT64_MAX) {
        fprintf(stderr, "latc: native guest has no loadable segments\n");
        g_free(data);
        return -1;
    }
    if (!digest_verified) {
        GChecksum *sum = g_checksum_new(G_CHECKSUM_SHA256);
        g_checksum_update(sum, (const guchar *)data, size);
        gsize digest_size = 32;
        g_checksum_get_digest(sum, digest, &digest_size);
        g_checksum_free(sum);
    }
    *entry = elf->e_entry;
    *guest = g_byte_array_new_take((guint8 *)data, size);
    return 0;
}

static int find_guest_load_bias(const GByteArray *guest,
                                const aot_header *header,
                                const aot_segment *segments,
                                uint64_t *load_bias)
{
    const Elf64_Ehdr *elf = (const void *)guest->data;
    if (elf->e_type == ET_EXEC) {
        *load_bias = 0;
        return 0;
    }
    if (elf->e_type != ET_DYN) {
        return -1;
    }

    const Elf64_Phdr *phdrs =
        (const void *)(guest->data + elf->e_phoff);
    bool found = false;
    uint64_t bias = 0;
    for (uint32_t i = 0; i < header->segments_num; i++) {
        const aot_segment *segment = &segments[i];
        for (uint16_t j = 0; j < elf->e_phnum; j++) {
            const Elf64_Phdr *phdr = &phdrs[j];
            if (phdr->p_type != PT_LOAD ||
                (phdr->p_offset & TARGET_PAGE_MASK) !=
                    segment->details.file_offset) {
                continue;
            }
            uint64_t preferred = phdr->p_vaddr & TARGET_PAGE_MASK;
            if (segment->details.seg_begin < preferred) {
                return -1;
            }
            uint64_t candidate = segment->details.seg_begin - preferred;
            if (found && candidate != bias) {
                return -1;
            }
            bias = candidate;
            found = true;
            break;
        }
    }
    if (!found) {
        return -1;
    }
    *load_bias = bias;
    return 0;
}

static uint32_t native_indirect_exit(const aot_tb *tb, const uint8_t *code)
{
    if (!(tb->bool_flags & IS_INDIRECT_JMP) ||
        (tb->bool_flags & IS_TU_JMP) || tb->jmp_indirect % 4 ||
        tb->jmp_indirect >= tb->tb_cache_size) {
        return 0;
    }
    uint32_t remaining = tb->tb_cache_size - tb->jmp_indirect;
    unsigned int words = lat_native_indirect_exit_words(
        code + tb->jmp_indirect, remaining);
    if (!words || remaining <= words * 4) {
        return 0;
    }
    return tb->jmp_indirect + 1;
}

static uint32_t native_conditional_exit(const aot_tb *tb,
                                        const uint8_t *code)
{
#ifdef CONFIG_LATX_BNE_B
    if ((tb->bool_flags & IS_TU_JMP) ||
        tb->jmp_reset_offset[0] == UINT16_MAX ||
        tb->jmp_reset_offset[1] == UINT16_MAX ||
        tb->first_jmp_align == UINT16_MAX) {
        return 0;
    }
    uint32_t preceding = 4;
#ifdef CONFIG_LATX_INSTS_PATTERN
    if (tb->eflags_target_arg[0] != UINT16_MAX) {
        preceding += 4;
    }
#endif
    if (tb->first_jmp_align < preceding) {
        return 0;
    }
    uint32_t offset = tb->first_jmp_align - preceding;
    if (offset % 4 || tb->tb_cache_size < 4 ||
        offset > tb->tb_cache_size - 4) {
        return 0;
    }
    uint32_t instruction;
    memcpy(&instruction, code + offset, sizeof(instruction));
    int64_t target = (int64_t)offset + (int16_t)(instruction >> 10) * 4;
    if (instruction >> 26 >= 0x16 && instruction >> 26 <= 0x1b &&
        target >= 0 && target <= tb->tb_cache_size - 4) {
        return offset + 1;
    }
#endif
    return 0;
}

static bool direct_tb_target(const aot_tb *tb, const aot_segment *segment,
        aot_rel_kind kind, uint64_t *guest_pc)
{
    int32_t offset = -1;
    bool exit_id_0 = kind == JIRL_EPILOGUE_RET_ID_0 ||
                     kind == B_EPILOGUE_RET_ID_0;
    bool exit_id_1 = kind == JIRL_EPILOGUE_RET_ID_1 ||
                     kind == B_EPILOGUE_RET_ID_1;
    if (exit_id_0) {
        offset = tb->next_tb_pc_offset >= 0 ?
            tb->next_tb_pc_offset : tb->target_tb_pc_offset;
    } else if (exit_id_1) {
        offset = tb->target_tb_pc_offset >= 0 ?
            tb->target_tb_pc_offset : tb->next_tb_pc_offset;
    }
    if (offset >= 0) {
        *guest_pc = segment->details.seg_begin + (uint32_t)offset;
        return true;
    }
    int exit_id = exit_id_1 ? 1 : 0;
    uint64_t current_pc = segment->details.seg_begin + tb->offset_in_segment;
    uint64_t target_pc = current_pc + tb->lazypc[exit_id];
    if ((!exit_id_0 && !exit_id_1) ||
        (!tb->lazypc[exit_id] && tb->tu_jmp[exit_id] == UINT16_MAX) ||
        target_pc < segment->details.seg_begin ||
        target_pc >= segment->details.seg_end) {
        return false;
    }
    *guest_pc = target_pc;
    return true;
}

typedef struct RelocationOffsetSet {
    uint8_t *bits;
    uint64_t maximum;
} RelocationOffsetSet;

static bool relocation_offset_contains(const RelocationOffsetSet *offsets,
                                       uint64_t code_offset)
{
    return code_offset <= offsets->maximum &&
        (offsets->bits[code_offset >> 3] & (1U << (code_offset & 7)));
}

static void relocation_offset_add(RelocationOffsetSet *offsets,
                                  uint64_t code_offset)
{
    if (code_offset <= offsets->maximum) {
        offsets->bits[code_offset >> 3] |= 1U << (code_offset & 7);
    }
}

static void append_native_relocation(GArray *output,
        RelocationOffsetSet *offsets,
        LatNativeRelocationV1 relocation)
{
    g_array_append_val(output, relocation);
    relocation_offset_add(offsets, relocation.code_offset);
}

static const void *native_constant_table(aot_rel_kind kind, size_t *size)
{
#define CONSTANT_TABLE(rel, table) \
    case rel: *size = sizeof(table); return table
    switch (kind) {
    CONSTANT_TABLE(LOAD_HOST_AES_TE_LATX, AES_Te_latx);
    CONSTANT_TABLE(LOAD_HOST_AES_SBOX_LATX, AES_sbox_latx);
    CONSTANT_TABLE(LOAD_HOST_AES_VPAES_LASX, AES_vpaes_lasx);
    CONSTANT_TABLE(LOAD_VPAES_ENC_TABLES, latx_vpaes_enc_tables);
    CONSTANT_TABLE(LOAD_VPAES_DEC_TABLES, latx_vpaes_dec_tables);
    CONSTANT_TABLE(LOAD_VPAES_KEYGEN_TABLES, latx_vpaes_keygen_tables);
    CONSTANT_TABLE(LOAD_VPAES_ENC_TABLES_XV, latx_vpaes_enc_tables_xv);
    CONSTANT_TABLE(LOAD_VPAES_DEC_TABLES_XV, latx_vpaes_dec_tables_xv);
    default: return NULL;
    }
#undef CONSTANT_TABLE
}

static int append_relocation(GArray *output, RelocationOffsetSet *offsets,
        GByteArray *constant_data, GHashTable *constant_offsets,
        uint64_t constant_base,
        const aot_rel *source,
        const aot_tb *tb, uint64_t tb_code_offset,
        const aot_segment *segment, uint64_t load_bias)
{
    LatNativeRelocationV1 relocation = {
        .code_offset = tb_code_offset + source->tc_offset,
        .slots = source->rel_slots_num,
        .physical_target_plus_one = 0,
    };
    uint64_t guest_pc;
    size_t constant_size = 0;
    const void *constant = native_constant_table(source->kind, &constant_size);
    if (constant) {
        gpointer key = GINT_TO_POINTER(source->kind);
        gpointer found = g_hash_table_lookup(constant_offsets, key);
        uint64_t offset;
        if (found) {
            offset = GPOINTER_TO_UINT(found) - 1;
        } else {
            offset = align_up(constant_data->len, 32);
            guint previous_size = constant_data->len;
            g_byte_array_set_size(constant_data, offset + constant_size);
            memset(constant_data->data + previous_size, 0,
                   offset - previous_size);
            memcpy(constant_data->data + offset, constant, constant_size);
            g_hash_table_insert(constant_offsets, key,
                                GUINT_TO_POINTER(offset + 1));
        }
        if ((uint64_t)source->extra_addend >= constant_size) {
            fprintf(stderr, "latc: invalid native constant table addend\n");
            return -1;
        }
        relocation.kind = LAT_NATIVE_RELOC_IMAGE_DATA;
        relocation.addend = constant_base + offset + source->extra_addend;
        relocation.reserved = constant_size - source->extra_addend;
    } else if (source->kind == LOAD_CALL_TARGET) {
        relocation.kind = LAT_NATIVE_RELOC_GUEST_ADDRESS;
        relocation.addend = segment->details.seg_begin + source->extra_addend;
    } else if (direct_tb_target(tb, segment, source->kind, &guest_pc)) {
        relocation.kind = LAT_NATIVE_RELOC_TB_TARGET;
        relocation.addend = guest_pc;
        relocation.target = native_semantic_flags(tb->cflags);
        relocation.reserved = runtime_symbol(source->kind);
    } else {
        int symbol = runtime_symbol(source->kind);
        if (symbol == LAT_NATIVE_SYMBOL_INVALID) {
            fprintf(stderr, "latc: unsupported native relocation kind %d\n",
                    source->kind);
            return -1;
        }
        relocation.kind = LAT_NATIVE_RELOC_RUNTIME_SYMBOL;
        relocation.target = symbol;
        relocation.addend = source->extra_addend;
    }
    if ((relocation.kind == LAT_NATIVE_RELOC_GUEST_ADDRESS ||
         relocation.kind == LAT_NATIVE_RELOC_TB_TARGET) &&
        relocation.addend < load_bias) {
        fprintf(stderr,
                "latc: native relocation target 0x%llx precedes load bias 0x%llx"
                " source=0x%llx cflags=0x%x kind=%d\n",
                (unsigned long long)relocation.addend,
                (unsigned long long)load_bias,
                (unsigned long long)(segment->details.seg_begin +
                                     tb->offset_in_segment),
                tb->cflags, source->kind);
        return -1;
    }
    if (relocation.kind == LAT_NATIVE_RELOC_GUEST_ADDRESS ||
        relocation.kind == LAT_NATIVE_RELOC_TB_TARGET) {
        relocation.addend -= load_bias;
    }
    append_native_relocation(output, offsets, relocation);
    return 0;
}

static void append_tu_relocations(GArray *output, RelocationOffsetSet *offsets,
        const aot_tb *tb, uint64_t tb_code_offset, uint64_t guest_pc)
{
#ifdef CONFIG_LATX_TU
    for (int edge = 0; edge < 2; edge++) {
        if (tb->tu_jmp[edge] == UINT16_MAX) continue;
        uint64_t code_offset = tb_code_offset + tb->tu_jmp[edge];
        if (relocation_offset_contains(offsets, code_offset)) continue;
        LatNativeRelocationV1 relocation = {
            .code_offset = code_offset,
            .addend = guest_pc + tb->lazypc[edge],
            .kind = LAT_NATIVE_RELOC_TB_TARGET,
            .target = native_semantic_flags(tb->cflags),
            .slots = 1,
            .physical_target_plus_one = 0,
        };
        append_native_relocation(output, offsets, relocation);
    }
#else
    (void)output;
    (void)offsets;
    (void)tb;
    (void)tb_code_offset;
    (void)guest_pc;
#endif
}

static void append_jrra_relocation(GArray *output,
        RelocationOffsetSet *offsets,
        const aot_tb *tb, uint64_t tb_code_offset, const aot_segment *segment,
        const GByteArray *guest, uint64_t load_bias)
{
#ifdef CONFIG_LATX_JRRA
    if (!tb->return_target_ptr_offset) {
        return;
    }
    uint64_t target_pc = segment->details.seg_begin + tb->next_86_pc_offset;
    if (!guest_executable_address(guest, target_pc, load_bias) ||
        target_pc < load_bias) {
        return;
    }
    LatNativeRelocationV1 relocation = {
        .code_offset = tb_code_offset + tb->return_target_ptr_offset,
        .addend = target_pc - load_bias,
        .kind = LAT_NATIVE_RELOC_JRRA_TARGET,
        .target = native_semantic_flags(tb->cflags),
        .slots = 4,
        .physical_target_plus_one = 0,
    };
    append_native_relocation(output, offsets, relocation);
#else
    (void)output;
    (void)offsets;
    (void)tb;
    (void)tb_code_offset;
    (void)segment;
    (void)guest;
#endif
}

static int decode_sleb128_checked(const uint8_t **cursor, const uint8_t *end,
                                  int64_t *value)
{
    const uint8_t *p = *cursor;
    uint64_t result = 0;
    unsigned int shift = 0;

    for (unsigned int i = 0; i < 10 && p < end; i++) {
        uint8_t byte = *p++;
        uint64_t payload = byte & 0x7f;
        if (shift == 63 && payload != 0 && payload != 0x7f) {
            return -1;
        }
        if (shift < 64) {
            result |= payload << shift;
        }
        shift += 7;
        if (!(byte & 0x80)) {
            if (shift < 64 && (byte & 0x40)) {
                result |= UINT64_MAX << shift;
            }
            *cursor = p;
            *value = (int64_t)result;
            return 0;
        }
    }
    return -1;
}

static int add_signed_u64(uint64_t *value, int64_t delta)
{
    if ((delta >= 0 && (uint64_t)delta > UINT64_MAX - *value) ||
        (delta < 0 && (uint64_t)(-(delta + 1)) + 1 > *value)) {
        return -1;
    }
    *value += delta;
    return 0;
}

static int append_tb_pc_map(GArray *output, const uint8_t *code,
                            uint64_t code_size, const aot_tb *tb,
                            uint64_t code_offset, uint64_t guest_pc,
                            uint64_t decode_limit, uint64_t executable_limit)
{
    if (!tb->icount || !tb->tb_cache_size) {
        return 0;
    }
    if (code_offset > code_size ||
        tb->tu_search_addr_offset > code_size - code_offset) {
        return -1;
    }
    uint64_t search_offset = code_offset + tb->tu_search_addr_offset;
    if (search_offset >= decode_limit || decode_limit > code_size ||
        executable_limit < code_offset || executable_limit > decode_limit) {
        return -1;
    }
    const uint8_t *cursor = code + search_offset;
    const uint8_t *end = code + decode_limit;
    uint64_t current_guest_pc = guest_pc;
    int64_t current_state = 0;
    uint64_t encoded_host_end = 0;
    guint first_map = output->len;
    bool truncated_unlink_stub = false;

    for (uint16_t i = 0; i < tb->icount; i++) {
        int64_t guest_delta;
        int64_t state_delta;
        int64_t host_delta;
        if (decode_sleb128_checked(&cursor, end, &guest_delta) ||
            decode_sleb128_checked(&cursor, end, &state_delta) ||
            decode_sleb128_checked(&cursor, end, &host_delta) ||
            host_delta < 0 ||
            add_signed_u64(&current_guest_pc, guest_delta) ||
            (uint64_t)host_delta > UINT64_MAX - encoded_host_end) {
            return -1;
        }
        if (state_delta > INT32_MAX || state_delta < INT32_MIN ||
            (state_delta > 0 && current_state > INT32_MAX - state_delta) ||
            (state_delta < 0 && current_state < INT32_MIN - state_delta)) {
            return -1;
        }
        current_state += state_delta;
        if (!host_delta) {
            continue;
        }
        uint64_t host_begin = encoded_host_end;
        encoded_host_end += host_delta;
        uint64_t host_end = encoded_host_end;
        if (host_begin >= tb->tb_cache_size) {
            break;
        }
        if (host_end > tb->tb_cache_size) {
            if (!(tb->bool_flags & IS_TU_TB) || truncated_unlink_stub ||
                host_begin >= tb->tb_cache_size) {
                return -1;
            }
            /* TU construction moves this terminal unlink stub out of the TB. */
            host_end = tb->tb_cache_size;
            truncated_unlink_stub = true;
        }
        if (code_offset + host_begin >= executable_limit) {
            break;
        }
        if (code_offset + host_end > executable_limit) {
            host_end = executable_limit - code_offset;
        }
        LatNativePcMapV2 map = {
            .guest_pc = current_guest_pc,
            .host_offset_begin = code_offset + host_begin,
            .host_offset_end = code_offset + host_end,
            .state_record_offset = (uint32_t)(int32_t)current_state,
            .flags = LAT_NATIVE_PC_MAP_DYNAMIC_STATE |
                (current_state ? LAT_NATIVE_PC_MAP_STACK_POINTER_DELTA : 0),
        };
        g_array_append_val(output, map);
    }
    if (output->len > first_map) {
        LatNativePcMapV2 *last = &g_array_index(
            output, LatNativePcMapV2, output->len - 1);
        last->host_offset_end = executable_limit;
    }
    return 0;
}

typedef struct NativeTuRange {
    uint64_t begin;
    uint64_t end;
    uint64_t search_begin;
} NativeTuRange;

static int collect_native_tu_ranges(const aot_tb *tbs, size_t tb_count,
                                    uint64_t code_size,
                                    uint64_t aot_code_offset,
                                    GHashTable **ranges_out,
                                    GPtrArray **storage_out)
{
    GHashTable *ranges = g_hash_table_new(g_int64_hash, g_int64_equal);
    GPtrArray *storage = g_ptr_array_new_with_free_func(g_free);

    for (size_t i = 0; i < tb_count; i++) {
        if (!tbs[i].is_first_tb) {
            continue;
        }
        if (tbs[i].tb_cache_offset < aot_code_offset ||
            tbs[i].offset_in_tu) {
            goto fail;
        }
        uint64_t begin = tbs[i].tb_cache_offset - aot_code_offset;
        uint64_t end = begin + tbs[i].tu_size;
        if (begin > code_size || end < begin || end > code_size ||
            g_hash_table_contains(ranges, &begin)) {
            goto fail;
        }
        NativeTuRange *range = g_new(NativeTuRange, 1);
        *range = (NativeTuRange) {
            .begin = begin,
            .end = end,
            .search_begin = end,
        };
        g_ptr_array_add(storage, range);
        g_hash_table_insert(ranges, &range->begin, range);
    }
    for (size_t i = 0; i < tb_count; i++) {
        if (tbs[i].tb_cache_offset < aot_code_offset) {
            goto fail;
        }
        uint64_t tb_begin = tbs[i].tb_cache_offset - aot_code_offset;
        if (tb_begin < tbs[i].offset_in_tu) {
            goto fail;
        }
        uint64_t tu_begin = tb_begin - tbs[i].offset_in_tu;
        NativeTuRange *range = g_hash_table_lookup(ranges, &tu_begin);
        uint64_t tb_end = tb_begin + tbs[i].tb_cache_size;
        uint64_t search_begin = tb_begin + tbs[i].tu_search_addr_offset;
        if (!range || tb_end < tb_begin || tb_end > range->end ||
            search_begin < tb_begin || search_begin > range->end) {
            goto fail;
        }
        range->search_begin = MIN(range->search_begin, search_begin);
    }
    *ranges_out = ranges;
    *storage_out = storage;
    return 0;

fail:
    g_hash_table_destroy(ranges);
    g_ptr_array_free(storage, TRUE);
    return -1;
}

static gint compare_native_tu_range(gconstpointer left, gconstpointer right)
{
    const NativeTuRange *a = *(const NativeTuRange *const *)left;
    const NativeTuRange *b = *(const NativeTuRange *const *)right;
    if (a->begin != b->begin) {
        return a->begin < b->begin ? -1 : 1;
    }
    return 0;
}

static int extract_native_tus(GArray *output, const aot_tb *tbs,
                              size_t tb_count, uint64_t code_size,
                              uint64_t aot_code_offset)
{
    GHashTable *ranges;
    GPtrArray *storage;
    if (collect_native_tu_ranges(tbs, tb_count, code_size, aot_code_offset,
                                 &ranges, &storage)) {
        return -1;
    }
    g_ptr_array_sort(storage, compare_native_tu_range);
    for (guint i = 0; i < storage->len; i++) {
        const NativeTuRange *range = g_ptr_array_index(storage, i);
        LatNativeTuV1 tu = {
            .code_offset = range->begin,
            .code_size = range->end - range->begin,
            .search_offset = range->search_begin,
        };
        g_array_append_val(output, tu);
    }
    g_hash_table_destroy(ranges);
    g_ptr_array_free(storage, TRUE);
    return output->len ? 0 : -1;
}

static int native_tu_index_for_offset(const GArray *tus, uint64_t offset)
{
    for (guint i = 0; i < tus->len; i++) {
        const LatNativeTuV1 *tu = &g_array_index(tus, LatNativeTuV1, i);
        if (offset >= tu->code_offset &&
            offset < tu->code_offset + tu->code_size) {
            return (int)i;
        }
    }
    return -1;
}

static int extract_native_pc_maps(GArray *output, const uint8_t *code,
                                  uint64_t code_size, const aot_header *header,
                                  const aot_segment *segments,
                                  const aot_tb *tbs, size_t tb_count,
                                  uint64_t aot_code_offset,
                                  uint64_t load_bias)
{
    size_t skipped = 0;
    GHashTable *ranges;
    GPtrArray *storage;
    if (collect_native_tu_ranges(tbs, tb_count, code_size, aot_code_offset,
                                 &ranges, &storage)) {
        return -1;
    }
    for (size_t j = 0; j < tb_count; j++) {
        const aot_segment *segment = find_tb_segment(header, segments,
                                                     &tbs[j]);
        if (!segment || tbs[j].tb_cache_offset < aot_code_offset) {
            g_hash_table_destroy(ranges);
            g_ptr_array_free(storage, TRUE);
            return -1;
        }
        uint64_t code_offset = tbs[j].tb_cache_offset - aot_code_offset;
        uint64_t tu_begin = code_offset - tbs[j].offset_in_tu;
        NativeTuRange *range = g_hash_table_lookup(ranges, &tu_begin);
        uint64_t guest_pc = segment->details.seg_begin +
                            tbs[j].offset_in_segment;
        if (!range || guest_pc < load_bias) {
            g_hash_table_destroy(ranges);
            g_ptr_array_free(storage, TRUE);
            return -1;
        }
        guest_pc -= load_bias;
        uint64_t executable_limit = code_offset + tbs[j].tb_cache_size;
        if (tbs[j].tu_search_addr_offset &&
            tbs[j].tu_search_addr_offset < tbs[j].tb_cache_size) {
            executable_limit = code_offset + tbs[j].tu_search_addr_offset;
        }
        guint first_map = output->len;
        if (append_tb_pc_map(output, code, code_size, &tbs[j], code_offset,
                             guest_pc, range->end, executable_limit)) {
            g_array_set_size(output, first_map);
            skipped++;
        }
    }
    g_hash_table_destroy(ranges);
    g_ptr_array_free(storage, TRUE);
    if (skipped) {
        fprintf(stderr,
                "latc: skipped %zu native TBs without stable PC maps\n",
                skipped);
    }
    g_array_sort(output, compare_native_pc_map);
    for (guint i = 1; i < output->len; i++) {
        const LatNativePcMapV2 *previous = &g_array_index(
            output, LatNativePcMapV2, i - 1);
        const LatNativePcMapV2 *current = &g_array_index(
            output, LatNativePcMapV2, i);
        if (previous->host_offset_end > current->host_offset_begin) {
            return -1;
        }
    }
    return 0;
}

static int strip_process_local_search_data(uint8_t *code, uint64_t code_size,
        const aot_tb *tbs, size_t tb_count, uint64_t aot_code_offset)
{
#ifdef CONFIG_LATX_TU
    GHashTable *ranges;
    GPtrArray *storage;
    if (collect_native_tu_ranges(tbs, tb_count, code_size, aot_code_offset,
                                 &ranges, &storage)) {
        fprintf(stderr, "latc: invalid TU search data range\n");
        return -1;
    }
    for (guint i = 0; i < storage->len; i++) {
        const NativeTuRange *range = g_ptr_array_index(storage, i);
        memset(code + range->search_begin, 0,
               range->end - range->search_begin);
    }
    g_hash_table_destroy(ranges);
    g_ptr_array_free(storage, TRUE);
#else
    (void)code;
    (void)code_size;
    (void)tbs;
    (void)tb_count;
    (void)aot_code_offset;
#endif
    return 0;
}

int latc_native_export(const char *path, const char *guest_path,
        const aot_header *header, const aot_segment *segments,
        const aot_tb *tbs, uintptr_t tb_table_end,
        const void *code, uint64_t code_size, uint64_t aot_code_offset)
{
    GByteArray *guest = NULL;
    GArray *native_tbs = NULL;
    GArray *native_relocations = NULL;
    RelocationOffsetSet relocation_offsets = {0};
    GArray *native_pc_maps = NULL;
    GArray *native_tus = NULL;
    uint8_t *native_code = NULL;
    GByteArray *constant_data = g_byte_array_new();
    GHashTable *constant_offsets = g_hash_table_new(g_direct_hash, g_direct_equal);
    uint64_t constant_base = align_up(code_size, 32);
    uint64_t guest_entry = 0;
    uint64_t guest_base = 0;
    uint64_t guest_load_bias = 0;
    uint8_t guest_digest[32];
    int result = -1;

    if (!path || !*path || !guest_path || !header || !segments || !tbs ||
        !code || (uintptr_t)tbs > tb_table_end ||
        (tb_table_end - (uintptr_t)tbs) % sizeof(*tbs)) {
        fprintf(stderr, "latc: invalid native export arguments\n");
        goto out;
    }
    int digest_verified = latc_bundle_verified_guest_digest(guest_digest);
    if (digest_verified < 0 ||
        copy_guest(guest_path, &guest, &guest_entry, &guest_base,
                   guest_digest, digest_verified == 1)) {
        goto out;
    }
    if (find_guest_load_bias(guest, header, segments, &guest_load_bias)) {
        fprintf(stderr, "latc: cannot determine native guest load bias\n");
        goto out;
    }
    native_tbs = g_array_new(FALSE, FALSE, sizeof(LatNativeTbV1));
    native_relocations = g_array_new(FALSE, FALSE,
                                     sizeof(LatNativeRelocationV1));
    relocation_offsets.maximum = code_size;
    relocation_offsets.bits = g_malloc0((code_size >> 3) + 1);
    native_pc_maps = g_array_new(FALSE, FALSE, sizeof(LatNativePcMapV2));
    native_tus = g_array_new(FALSE, FALSE, sizeof(LatNativeTuV1));
    size_t tb_count = (tb_table_end - (uintptr_t)tbs) / sizeof(*tbs);
    native_code = g_malloc(code_size);
    memcpy(native_code, code, code_size);
    if (extract_native_pc_maps(native_pc_maps, native_code, code_size,
            header, segments, tbs, tb_count, aot_code_offset,
            guest_load_bias) ||
        extract_native_tus(native_tus, tbs, tb_count, code_size,
                           aot_code_offset) ||
        strip_process_local_search_data(native_code, code_size,
            tbs, tb_count, aot_code_offset)) {
        fprintf(stderr, "latc: cannot export stable native PC map\n");
        goto out;
    }
    const aot_rel *source_relocations =
        (const void *)((const uint8_t *)header + header->rel_table_offset);

    for (size_t i = 0; i < tb_count; i++) {
        const aot_segment *segment = find_tb_segment(header, segments, &tbs[i]);
        uint64_t pc = segment ?
            segment->details.seg_begin + tbs[i].offset_in_segment : 0;
        if (!segment || pc < guest_load_bias ||
            tbs[i].tb_cache_offset < aot_code_offset) {
            fprintf(stderr, "latc: invalid native TB %zu\n", i);
            goto out;
        }
        uint64_t code_offset = tbs[i].tb_cache_offset - aot_code_offset;
        if (!tbs[i].tb_cache_size) {
            continue;
        }
        if (code_offset > code_size ||
            tbs[i].tb_cache_size > code_size - code_offset) {
            fprintf(stderr, "latc: native TB %zu code range is invalid\n", i);
            goto out;
        }
        int tu_index = native_tu_index_for_offset(native_tus, code_offset);
        if (tu_index < 0) {
            fprintf(stderr, "latc: native TB %zu is outside a TU\n", i);
            goto out;
        }
        uint64_t executable_end = code_offset + tbs[i].tb_cache_size;
        if (tbs[i].tu_search_addr_offset &&
            tbs[i].tu_search_addr_offset < tbs[i].tb_cache_size) {
            executable_end = code_offset + tbs[i].tu_search_addr_offset;
        }
        if (executable_end <= code_offset) {
            fprintf(stderr, "latc: native TB %zu has no executable range\n", i);
            goto out;
        }
        LatNativeTbV1 native_tb = {
            .guest_pc = pc - guest_load_bias,
            .code_offset = code_offset,
            .code_size = executable_end - code_offset,
            .flags = native_semantic_flags(tbs[i].cflags),
            .conditional_exit_offset = native_conditional_exit(
                &tbs[i], native_code + code_offset),
            .indirect_exit_offset = native_indirect_exit(
                &tbs[i], native_code + code_offset),
            .tu_index = (uint32_t)tu_index,
        };
#ifdef CONFIG_LATX_INSTS_PATTERN
        if (!tbs[i].eflag_use) {
            native_tb.optimization_flags = LAT_NATIVE_TB_ENTRY_FLAGS_DEAD;
        }
        for (int edge = 0; edge < 2; edge++) {
            uint32_t offset = tbs[i].eflags_target_arg[edge];
            uint16_t stub = UINT16_MAX;
#ifdef CONFIG_LATX_XCOMISX_OPT
            stub = tbs[i].jmp_stub_reset_offset[edge];
#endif
            if (tbs[i].bool_flags & IS_TU_JMP) {
                continue;
            }
            unsigned actions = lat_eflags_link_actions(0,
                edge && (tbs[i].bool_flags & OPT_BCC), offset, stub);
            if (actions & LAT_EFLAGS_LINK_NOP) {
                uint32_t backup = tbs[i].eflags_target_arg[EFLAG_BACKUP];
                uint32_t instruction, original;
                if (offset % 4 || offset + 4 > native_tb.code_size ||
                    backup == UINT16_MAX || backup % 4 ||
                    backup + 4 > native_tb.code_size) {
                    fprintf(stderr, "latc: invalid eflags patch site\n");
                    goto out;
                }
                memcpy(&instruction, native_code + code_offset + offset, 4);
                memcpy(&original, native_code + code_offset + backup, 4);
                if (instruction != original) {
                    fprintf(stderr, "latc: eflags patch differs from backup\n");
                    goto out;
                }
                native_tb.eflags_instruction = original;
                native_tb.eflags_offset[edge] = offset + 1;
            }
#ifdef CONFIG_LATX_XCOMISX_OPT
            if (actions & LAT_EFLAGS_LINK_BYPASS) {
                uint64_t site = tbs[i].jmp_stub_target_arg[edge];
                uint32_t instruction;
                if (site % 4 || site + 4 > native_tb.code_size) {
                    fprintf(stderr, "latc: invalid eflags stub site\n");
                    goto out;
                }
                memcpy(&instruction, native_code + code_offset + site, 4);
                if (instruction != 0x03400000u) {
                    fprintf(stderr, "latc: eflags stub is not unlinked\n");
                    goto out;
                }
                native_tb.eflags_stub_offset[edge] = site + 1;
            }
#endif
        }
#endif
        g_array_append_val(native_tbs, native_tb);

        if (tbs[i].rel_start_index == -1) {
            append_tu_relocations(native_relocations, &relocation_offsets,
                                  &tbs[i], code_offset,
                                  pc - guest_load_bias);
            append_jrra_relocation(native_relocations, &relocation_offsets,
                                   &tbs[i], code_offset,
                                   segment, guest, guest_load_bias);
            continue;
        }
        if (tbs[i].rel_start_index < 0 ||
            tbs[i].rel_end_index < tbs[i].rel_start_index ||
            (uint32_t)tbs[i].rel_end_index >= header->rel_entry_num) {
            fprintf(stderr, "latc: native TB %zu relocation range is invalid\n", i);
            goto out;
        }
        for (int rel = tbs[i].rel_start_index;
             rel <= tbs[i].rel_end_index; rel++) {
            if (append_relocation(native_relocations, &relocation_offsets,
                                  constant_data, constant_offsets, constant_base,
                                  &source_relocations[rel],
                                  &tbs[i], code_offset, segment,
                                  guest_load_bias)) {
                goto out;
            }
        }
        append_tu_relocations(native_relocations, &relocation_offsets,
                              &tbs[i], code_offset,
                              pc - guest_load_bias);
        append_jrra_relocation(native_relocations, &relocation_offsets,
                               &tbs[i], code_offset,
                               segment, guest, guest_load_bias);
    }

    sort_native_tbs(native_tbs);
    for (guint i = 1; i < native_tbs->len; i++) {
        const LatNativeTbV1 *previous = &g_array_index(
            native_tbs, LatNativeTbV1, i - 1);
        const LatNativeTbV1 *current = &g_array_index(
            native_tbs, LatNativeTbV1, i);
        if (previous->guest_pc == current->guest_pc &&
            previous->flags == current->flags) {
            fprintf(stderr, "latc: duplicate native TB pc=0x%llx flags=0x%x\n",
                    (unsigned long long)current->guest_pc, current->flags);
            goto out;
        }
    }
    GArray *native_tb_code = g_array_sized_new(
        FALSE, FALSE, sizeof(const LatNativeTbV1 *), native_tbs->len);
    for (guint i = 0; i < native_tbs->len; i++) {
        const LatNativeTbV1 *tb = &g_array_index(
            native_tbs, LatNativeTbV1, i);
        g_array_append_val(native_tb_code, tb);
    }
    g_array_sort(native_tb_code, compare_native_tb_code);
    for (guint i = 0; i < native_relocations->len; i++) {
        LatNativeRelocationV1 *relocation = &g_array_index(
            native_relocations, LatNativeRelocationV1, i);
        if (relocation->kind != LAT_NATIVE_RELOC_TB_TARGET ||
            relocation->slots < 1 || relocation->slots > 2) {
            continue;
        }
        uint32_t words[2] = {0};
        memcpy(words, native_code + relocation->code_offset,
               relocation->slots * sizeof(uint32_t));
        uint64_t local_target = 0;
        if (!lat_native_decode_local_branch_target(
                words, relocation->slots, relocation->code_offset,
                code_size, &local_target)) {
            continue;
        }
        int target_index = native_tb_index_for_code(
            native_tbs, native_tb_code, local_target);
        if (target_index >= 0 &&
            g_array_index(native_tbs, LatNativeTbV1, target_index).code_offset ==
                local_target) {
            relocation->physical_target_plus_one =
                (uint32_t)target_index + 1;
        }
    }
    g_array_free(native_tb_code, TRUE);

    for (guint i = 0; i < native_relocations->len; i++) {
        LatNativeRelocationV1 *relocation = &g_array_index(
            native_relocations, LatNativeRelocationV1, i);
        if (relocation->kind != LAT_NATIVE_RELOC_TB_TARGET ||
            guest_executable_address(guest, relocation->addend, 0)) {
            continue;
        }
        if (relocation->reserved == LAT_NATIVE_SYMBOL_INVALID) {
            fprintf(stderr,
                    "latc: native TB target 0x%llx is outside executable segments\n",
                    (unsigned long long)relocation->addend);
            goto out;
        }
        relocation->kind = LAT_NATIVE_RELOC_RUNTIME_SYMBOL;
        relocation->target = relocation->reserved;
        relocation->reserved = 0;
        relocation->addend = 0;
    }

    GArray *missing_targets = g_array_new(FALSE, FALSE, sizeof(uint64_t));
    GHashTable *missing_seen = g_hash_table_new(g_direct_hash, g_direct_equal);
    for (guint i = 0; i < native_relocations->len; i++) {
        const LatNativeRelocationV1 *relocation = &g_array_index(
            native_relocations, LatNativeRelocationV1, i);
        if ((relocation->kind == LAT_NATIVE_RELOC_TB_TARGET ||
             relocation->kind == LAT_NATIVE_RELOC_JRRA_TARGET) &&
            !native_tb_target_exists(native_tbs,
                                     (uint64_t)relocation->addend,
                                     relocation->target)) {
            gpointer key = (gpointer)(uintptr_t)relocation->addend;
            if (!g_hash_table_contains(missing_seen, key)) {
                uint64_t pc = relocation->addend;
                g_hash_table_add(missing_seen, key);
                g_array_append_val(missing_targets, pc);
            }
        }
    }
    g_hash_table_destroy(missing_seen);
    if (missing_targets->len) {
        g_array_sort(missing_targets, compare_uint64);
        const char *missing_path = getenv("LATC_NATIVE_MISSING_OUT");
        if (missing_path && *missing_path) {
            FILE *missing = fopen(missing_path, "w");
            if (!missing) {
                fprintf(stderr, "latc: cannot create missing-target list %s: %s\n",
                        missing_path, strerror(errno));
                g_array_free(missing_targets, TRUE);
                goto out;
            }
            for (guint i = 0; i < missing_targets->len; i++) {
                fprintf(missing, "0x%llx 1\n", (unsigned long long)
                        g_array_index(missing_targets, uint64_t, i));
            }
            if (fclose(missing)) {
                fprintf(stderr, "latc: cannot write missing-target list %s\n",
                        missing_path);
                g_array_free(missing_targets, TRUE);
                goto out;
            }
        }
        fprintf(stderr, "latc: native image has %u missing TB targets; first pc=0x%llx\n",
                missing_targets->len, (unsigned long long)
                g_array_index(missing_targets, uint64_t, 0));
        bool allow_missing = getenv("LATC_NATIVE_ALLOW_MISSING") != NULL;
        g_array_free(missing_targets, TRUE);
        if (!allow_missing) {
            goto out;
        }
        fprintf(stderr,
                "latc: preserving unresolved targets for module JIT fallback\n");
    } else {
        g_array_free(missing_targets, TRUE);
    }

    LatNativeImageHeaderV2 native_header = {0};
    memcpy(native_header.magic, LAT_NATIVE_IMAGE_MAGIC, 8);
    native_header.version = LAT_NATIVE_IMAGE_VERSION;
    native_header.header_size = sizeof(native_header);
    const Elf64_Ehdr *guest_elf = (const void *)guest->data;
    native_header.flags = (guest_elf->e_type == ET_DYN ?
                               LAT_NATIVE_IMAGE_PIE : 0) |
                          LAT_NATIVE_IMAGE_NEEDS_FALLBACK |
                          LAT_NATIVE_IMAGE_LBT | LAT_NATIVE_IMAGE_LSX |
                          LAT_NATIVE_IMAGE_NO_PRECISE_SIGNAL_MAP;
    native_header.guest_entry = guest_entry;
    native_header.preferred_guest_base = guest_base;
    native_header.guest_image_offset = sizeof(native_header);
    native_header.guest_image_size = guest->len;
    native_header.code_offset = align_up(native_header.guest_image_offset +
                                         native_header.guest_image_size, 8);
    native_header.code_size = constant_data->len ?
        constant_base + constant_data->len : code_size;
    native_header.tb_table_offset = align_up(native_header.code_offset +
                                             native_header.code_size, 8);
    native_header.tb_count = native_tbs->len;
    native_header.relocation_offset = native_header.tb_table_offset +
        native_tbs->len * sizeof(LatNativeTbV1);
    native_header.relocation_count = native_relocations->len;
    native_header.pc_map_offset = native_header.relocation_offset +
        native_relocations->len * sizeof(LatNativeRelocationV1);
    native_header.pc_map_count = native_pc_maps->len;
    native_header.tu_table_offset = native_header.pc_map_offset +
        native_header.pc_map_count * sizeof(LatNativePcMapV2);
    native_header.tu_count = native_tus->len;
    memcpy(native_header.guest_sha256, guest_digest, sizeof(guest_digest));
    snprintf(native_header.lat_build_id, sizeof(native_header.lat_build_id),
             "%s", LATC_BUILD_ID);

    FILE *output = fopen(path, "wb");
    if (!output) {
        fprintf(stderr, "latc: cannot create native image %s: %s\n",
                path, strerror(errno));
        goto out;
    }
    if (fwrite(&native_header, sizeof(native_header), 1, output) != 1 ||
        fwrite(guest->data, guest->len, 1, output) != 1) {
        goto write_error;
    }
    uint64_t position = sizeof(native_header) + guest->len;
    static const uint8_t zero[8] = {0};
    if (native_header.code_offset > position &&
        fwrite(zero, native_header.code_offset - position, 1, output) != 1) {
        goto write_error;
    }
    if (fwrite(native_code, code_size, 1, output) != 1) {
        goto write_error;
    }
    if (constant_data->len) {
        uint8_t zero = 0;
        for (uint64_t i = code_size; i < constant_base; i++) {
            if (fwrite(&zero, 1, 1, output) != 1) goto write_error;
        }
        if (fwrite(constant_data->data, constant_data->len, 1, output) != 1) {
            goto write_error;
        }
    }
    position = native_header.code_offset + native_header.code_size;
    if (native_header.tb_table_offset > position &&
        fwrite(zero, native_header.tb_table_offset - position, 1, output) != 1) {
        goto write_error;
    }
    if ((native_tbs->len && fwrite(native_tbs->data,
            sizeof(LatNativeTbV1), native_tbs->len, output) != native_tbs->len) ||
        (native_relocations->len && fwrite(native_relocations->data,
            sizeof(LatNativeRelocationV1), native_relocations->len, output) !=
            native_relocations->len) ||
        (native_pc_maps->len && fwrite(native_pc_maps->data,
            sizeof(LatNativePcMapV2), native_pc_maps->len, output) !=
            native_pc_maps->len) ||
        (native_tus->len && fwrite(native_tus->data,
            sizeof(LatNativeTuV1), native_tus->len, output) !=
            native_tus->len)) {
        goto write_error;
    }
    if (fclose(output)) {
        goto write_error_unclosed;
    }
    result = 0;
    goto out;

write_error:
    fclose(output);
write_error_unclosed:
    unlink(path);
    fprintf(stderr, "latc: cannot write native image %s\n", path);
out:
    g_byte_array_unref(constant_data);
    g_hash_table_destroy(constant_offsets);
    g_free(relocation_offsets.bits);
    if (native_tbs) g_array_free(native_tbs, TRUE);
    if (native_relocations) g_array_free(native_relocations, TRUE);
    if (native_pc_maps) g_array_free(native_pc_maps, TRUE);
    if (native_tus) g_array_free(native_tus, TRUE);
    g_free(native_code);
    if (guest) g_byte_array_unref(guest);
    return result;
}
