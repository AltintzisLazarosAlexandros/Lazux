#pragma once
#include <stdint.h>

#define RAMDISK_MAGIC 0x4C415A55 // "LAZU" in ASCII

typedef struct {
    char name[32];      /* File name (null-terminated string) */
    uint32_t offset;    /* Offset in bytes from the start of the ramdisk image to the file data */
    uint32_t size;      /* Size of the file in bytes */
} ramdisk_entry_t;

/* The header (appears at the beginning of the ramdisk.img) */
typedef struct {
    uint32_t magic;       /* Must always be RAMDISK_MAGIC */
    uint32_t num_entries; /* Number of ramdisk_entry_t entries that follow */
} ramdisk_header_t;