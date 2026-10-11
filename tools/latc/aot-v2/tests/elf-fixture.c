#include "elf-fixture.h"

#include <elf.h>
#include <string.h>

typedef struct TestNote {
    Elf64_Nhdr header;
    char name[4];
    LatAotNoteV2 description;
} TestNote;

void lat_aot_test_build_elf(unsigned char file[LAT_AOT_TEST_FILE_SIZE])
{
    memset(file, 0, LAT_AOT_TEST_FILE_SIZE);
    Elf64_Ehdr *header = (void *)file;
    memcpy(header->e_ident, ELFMAG, SELFMAG);
    header->e_ident[EI_CLASS] = ELFCLASS64;
    header->e_ident[EI_DATA] = ELFDATA2LSB;
    header->e_ident[EI_VERSION] = EV_CURRENT;
    header->e_type = ET_DYN;
    header->e_machine = EM_LOONGARCH;
    header->e_version = EV_CURRENT;
    header->e_ehsize = sizeof(*header);
    header->e_phoff = sizeof(*header);
    header->e_phentsize = sizeof(Elf64_Phdr);
    header->e_phnum = 4;
    header->e_shoff = LAT_AOT_TEST_SECTION_OFFSET;
    header->e_shentsize = sizeof(Elf64_Shdr);
    header->e_shnum = 5;

    Elf64_Phdr *phdrs = (void *)(file + header->e_phoff);
    phdrs[0] = (Elf64_Phdr) {
        .p_type = PT_LOAD, .p_flags = PF_R | PF_X, .p_offset = 0,
        .p_vaddr = 0, .p_filesz = LAT_AOT_TEST_FILE_SIZE,
        .p_memsz = LAT_AOT_TEST_FILE_SIZE, .p_align = 0x1000,
    };
    phdrs[1] = (Elf64_Phdr) {
        .p_type = PT_NOTE, .p_flags = PF_R,
        .p_offset = LAT_AOT_TEST_NOTE_OFFSET,
        .p_vaddr = LAT_AOT_TEST_NOTE_OFFSET,
        .p_filesz = sizeof(TestNote), .p_memsz = sizeof(TestNote), .p_align = 4,
    };
    phdrs[2] = (Elf64_Phdr) {
        .p_type = PT_DYNAMIC, .p_flags = PF_R,
        .p_offset = LAT_AOT_TEST_DYNAMIC_OFFSET,
        .p_vaddr = LAT_AOT_TEST_DYNAMIC_OFFSET,
        .p_filesz = 5 * sizeof(Elf64_Dyn),
        .p_memsz = 5 * sizeof(Elf64_Dyn), .p_align = 8,
    };
    phdrs[3] = (Elf64_Phdr) {
        .p_type = PT_LOAD, .p_flags = PF_R | PF_W,
        .p_offset = LAT_AOT_TEST_RELOCATION_OFFSET, .p_vaddr = 0x1700,
        .p_filesz = 0x100, .p_memsz = 0x100, .p_align = 0x1000,
    };

    TestNote *note = (void *)(file + LAT_AOT_TEST_NOTE_OFFSET);
    note->header.n_namesz = sizeof(LAT_AOT_V2_NOTE_NAME);
    note->header.n_descsz = sizeof(LatAotNoteV2);
    note->header.n_type = LAT_AOT_V2_NOTE_TYPE;
    memcpy(note->name, LAT_AOT_V2_NOTE_NAME, sizeof(LAT_AOT_V2_NOTE_NAME));
    memcpy(note->description.magic, "LATAOT2", 8);
    note->description.abi_version = LAT_AOT_V2_ABI_VERSION;
    note->description.struct_size = sizeof(LatAotNoteV2);
    note->description.module_flags = LAT_AOT_MODULE_READONLY_TEXT |
                                     LAT_AOT_MODULE_SYNTHETIC_FIXTURE;
    note->description.required_features = LAT_AOT_V2_REQUIRED_BASE_FEATURES;
    memset(note->description.source_sha256, 0x11, 32);
    memset(note->description.codegen_id, 0x22, 32);
    memset(note->description.tbset_digest, 0x33, 32);

    char *strings = (void *)(file + LAT_AOT_TEST_STRING_OFFSET);
    size_t needed = 1;
    strcpy(strings + needed, LAT_AOT_V2_RUNTIME_SONAME);
    size_t runtime = needed + strlen(strings + needed) + 1;
    strcpy(strings + runtime, LAT_AOT_V2_RUNTIME_ABI_SYMBOL);
    size_t descriptor = runtime + strlen(strings + runtime) + 1;
    strcpy(strings + descriptor, LAT_AOT_V2_DESCRIPTOR_SYMBOL);
    size_t string_size = descriptor + strlen(strings + descriptor) + 1;

    Elf64_Dyn *dynamic = (void *)(file + LAT_AOT_TEST_DYNAMIC_OFFSET);
    dynamic[0] = (Elf64_Dyn) { .d_tag = DT_STRTAB,
                               .d_un.d_ptr = LAT_AOT_TEST_STRING_OFFSET };
    dynamic[1] = (Elf64_Dyn) { .d_tag = DT_STRSZ,
                               .d_un.d_val = string_size };
    dynamic[2] = (Elf64_Dyn) { .d_tag = DT_NEEDED, .d_un.d_val = needed };
    dynamic[3] = (Elf64_Dyn) { .d_tag = DT_NULL };

    Elf64_Sym *symbols = (void *)(file + LAT_AOT_TEST_SYMBOL_OFFSET);
    symbols[1] = (Elf64_Sym) {
        .st_name = runtime, .st_info = (STB_GLOBAL << 4) | STT_FUNC,
        .st_shndx = SHN_UNDEF,
    };
    symbols[2] = (Elf64_Sym) {
        .st_name = descriptor, .st_info = (STB_GLOBAL << 4) | STT_OBJECT,
        .st_shndx = 3,
    };

    Elf64_Shdr *sections = (void *)(file + LAT_AOT_TEST_SECTION_OFFSET);
    sections[1] = (Elf64_Shdr) {
        .sh_type = SHT_STRTAB, .sh_flags = SHF_ALLOC,
        .sh_addr = LAT_AOT_TEST_STRING_OFFSET,
        .sh_offset = LAT_AOT_TEST_STRING_OFFSET, .sh_size = string_size,
        .sh_addralign = 1,
    };
    sections[2] = (Elf64_Shdr) {
        .sh_type = SHT_DYNSYM, .sh_flags = SHF_ALLOC,
        .sh_addr = LAT_AOT_TEST_SYMBOL_OFFSET,
        .sh_offset = LAT_AOT_TEST_SYMBOL_OFFSET,
        .sh_size = 3 * sizeof(Elf64_Sym), .sh_link = 1, .sh_info = 1,
        .sh_addralign = 8, .sh_entsize = sizeof(Elf64_Sym),
    };
    sections[3] = (Elf64_Shdr) {
        .sh_type = SHT_PROGBITS, .sh_flags = SHF_ALLOC | SHF_EXECINSTR,
        .sh_addr = LAT_AOT_TEST_TEXT_OFFSET,
        .sh_offset = LAT_AOT_TEST_TEXT_OFFSET, .sh_size = 4, .sh_addralign = 4,
    };
    sections[4] = (Elf64_Shdr) {
        .sh_type = SHT_RELA, .sh_flags = SHF_ALLOC, .sh_addr = 0x1700,
        .sh_offset = LAT_AOT_TEST_RELOCATION_OFFSET,
        .sh_size = sizeof(Elf64_Rela), .sh_link = 2, .sh_info = 3,
        .sh_addralign = 8, .sh_entsize = sizeof(Elf64_Rela),
    };
    Elf64_Rela *relocation =
        (void *)(file + LAT_AOT_TEST_RELOCATION_OFFSET);
    relocation->r_offset = 0x1700;
    relocation->r_info = 3;
}

void lat_aot_test_expected(LatAotExpectedV2 *expected)
{
    memset(expected, 0, sizeof(*expected));
    expected->available_features = LAT_AOT_V2_REQUIRED_BASE_FEATURES;
    memset(expected->source_sha256, 0x11, 32);
    memset(expected->codegen_id, 0x22, 32);
}
