/*
 * elf.h - ELF64 Binary Format Definitions
 * 
 * Defines structures for parsing ELF64 (Executable and Linkable Format) binaries.
 * User programs are compiled to ELF64 and embedded in the kernel as raw binary data.
 * The kernel parses these headers to understand where code/data segments are located
 * and loads them into isolated process memory spaces.
 */

#pragma once

#include <stdint.h>

/* ELF magic number: first 4 bytes of every ELF file are 0x7f, 'E', 'L', 'F' */
#define ELF_NUM 0x464C457FU
/* Program header type: PT_LOAD = loadable segment (code/data) */
#define PT_LOAD 1

/*
 * elf64_ehdr_t - ELF64 File Header
 * 
 * Located at the beginning of every ELF file.
 * Describes the file structure, machine type, program headers, etc.
 */
typedef struct {
    uint8_t  e_ident[16];    /* Identification: magic number, class (32/64-bit), endianness, version */
    uint16_t e_type;         /* File type (executable, shared object, etc.) */
    uint16_t e_machine;      /* Target machine (RISC-V = 0xF3 / 243) */
    uint32_t e_version;      /* ELF version */
    uint64_t e_entry;        /* Virtual address of entry point (where to start execution) */
    uint64_t e_phoff;        /* Byte offset to program header table */
    uint64_t e_shoff;        /* Byte offset to section header table (not used by loader) */
    uint32_t e_flags;        /* Machine-specific flags */
    uint16_t e_ehsize;       /* Size of this ELF header */
    uint16_t e_phentsize;    /* Size of one program header entry */
    uint16_t e_phnum;        /* Number of program header entries */
    uint16_t e_shentsize;    /* Size of one section header entry */
    uint16_t e_shnum;        /* Number of section header entries */
    uint16_t e_shstrndx;     /* Index of string table section */
} elf64_ehdr_t;

/*
 * elf64_phdr_t - ELF64 Program Header
 * 
 * Describes a loadable segment (code, data, BSS) within the ELF file.
 * The kernel iterates through program headers to find all PT_LOAD segments
 * and maps them into the process's virtual address space.
 */
typedef struct {
    uint32_t p_type;         /* Segment type (PT_LOAD = loadable, PT_DYNAMIC = dynamic linking, etc.) */
    uint32_t p_flags;        /* Segment permissions (PF_R=readable, PF_W=writable, PF_X=executable) */
    uint64_t p_offset;       /* Byte offset of segment data in the ELF file */
    uint64_t p_vaddr;        /* Virtual address where segment should be loaded */
    uint64_t p_paddr;        /* Physical address (not used in virtual memory systems) */
    uint64_t p_filesz;       /* Number of bytes to copy from the ELF file into memory */
    uint64_t p_memsz;        /* Total size of segment in memory (p_memsz >= p_filesz; gap is BSS) */
    uint64_t p_align;        /* Alignment requirement for virtual address */
} elf64_phdr_t;
