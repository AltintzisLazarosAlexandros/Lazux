#include "../kernel/include/mkramdisk.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


const char* get_filename(const char* path) {
    const char* p = path + strlen(path);
    while (p > path && *(p - 1) != '/') {
        p--;
    }
    return p;
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        printf("Usage: %s <output.img> <file1> <file2> ...\n", argv[0]);
        return 1;
    }

    const char* output_file = argv[1];
    int num_files = argc - 2;

    FILE *out = fopen(output_file, "wb");
    if (!out) {
        printf("Error: Could not open output file %s\n", output_file);
        return 1;
    }

    ramdisk_header_t header;
    header.magic = RAMDISK_MAGIC;
    header.num_entries = num_files;
    fwrite(&header, sizeof(ramdisk_header_t), 1, out);

    ramdisk_entry_t *entries = malloc(num_files * sizeof(ramdisk_entry_t));

    uint32_t current_offset = sizeof(ramdisk_header_t) + (num_files * sizeof(ramdisk_entry_t));

    for (int i = 0; i < num_files; i++) {
        const char* input_path = argv[i + 2];
        FILE *in = fopen(input_path, "rb");
        if (!in) {
            printf("Error: Could not open input file %s\n", input_path);
            return 1;
        }

        fseek(in, 0, SEEK_END);
        uint32_t size = ftell(in);
        fclose(in);

        strncpy(entries[i].name, get_filename(input_path), 31);
        entries[i].name[31] = '\0'; 
        entries[i].offset = current_offset;
        entries[i].size = size;

        current_offset += size;
    }

    fwrite(entries, sizeof(ramdisk_entry_t), num_files, out);

    for (int i = 0; i < num_files; i++) {
        const char* input_path = argv[i + 2];
        FILE *in = fopen(input_path, "rb");
        
        char buffer[4096];
        size_t bytes_read;
        while ((bytes_read = fread(buffer, 1, sizeof(buffer), in)) > 0) {
            fwrite(buffer, 1, bytes_read, out);
        }
        fclose(in);
        printf("Added: %s (Offset: %u, Size: %u bytes)\n", entries[i].name, entries[i].offset, entries[i].size);
    }

    free(entries);
    fclose(out);
    printf("Successfully created %s with %d files.\n", output_file, num_files);

    return 0;
}