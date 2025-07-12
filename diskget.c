#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include "fs.h"

// Copy the same locate_dir implementation from disklist.c:
static int locate_dir(FILE *fp,
                      const superblock_t *sb,
                      const char *path,
                      uint32_t *out_start,
                      uint32_t *out_blocks)
{
    if (strcmp(path, "/") == 0) {
        *out_start  = sb->root_start;
        *out_blocks = sb->root_blocks;
        return 0;
    }

    char *tmp = strdup(path + 1);
    if (!tmp) { perror("strdup"); return -1; }

    uint32_t cur_start  = sb->root_start;
    uint32_t cur_blocks = sb->root_blocks;
    char *token = strtok(tmp, "/");

    while (token) {
        dir_entry_t *ents = NULL;
        int n = read_dir_entries(fp, sb, cur_start, cur_blocks, &ents);
        if (n < 0) { free(tmp); return -1; }

        int found = 0;
        for (int i = 0; i < n; i++) {
            if ((ents[i].status & 0x4) &&
                strcmp(ents[i].name, token) == 0)
            {
                cur_start  = ents[i].start_block;
                cur_blocks = ents[i].block_count;
                found = 1;
                break;
            }
        }
        free(ents);
        if (!found) {
            fprintf(stderr, "Directory not found.\n");
            free(tmp);
            return -1;
        }
        token = strtok(NULL, "/");
    }

    free(tmp);
    *out_start  = cur_start;
    *out_blocks = cur_blocks;
    return 0;
}

int main(int argc, char **argv) {
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <image> <fs_path> <host_dest>\n", argv[0]);
        return 1;
    }

    const char *img_path = argv[1];
    const char *fs_path  = argv[2];
    const char *out_path = argv[3];

    FILE *img = fopen(img_path, "rb");
    if (!img) { perror("fopen"); return 1; }

    superblock_t sb;
    if (read_superblock(img, &sb) != 0) {
        fprintf(stderr, "Error reading superblock\n");
        fclose(img);
        return 1;
    }

    // Split fs_path into parent-dir and basename
    char *dup = strdup(fs_path);
    if (!dup) { perror("strdup"); fclose(img); return 1; }
    char *slash = strrchr(dup, '/');
    char *dir_path, *file_name;
    if (slash == dup) {
        dir_path  = "/";
        file_name = dup + 1;
    } else {
        *slash    = '\0';
        dir_path  = dup;
        file_name = slash + 1;
    }

    // Locate parent directory
    uint32_t dir_start, dir_blocks;
    if (locate_dir(img, &sb, dir_path, &dir_start, &dir_blocks) != 0) {
        free(dup);
        fclose(img);
        return 1;
    }

    // Find the file entry
    dir_entry_t *ents = NULL;
    int n = read_dir_entries(img, &sb, dir_start, dir_blocks, &ents);
    if (n < 0) {
        fprintf(stderr, "Error reading directory entries\n");
        free(dup);
        fclose(img);
        return 1;
    }

    dir_entry_t *file_ent = NULL;
    for (int i = 0; i < n; i++) {
        if ((ents[i].status & 0x1) && (ents[i].status & 0x2) &&
            strcmp(ents[i].name, file_name) == 0)
        {
            file_ent = &ents[i];
            break;
        }
    }
    free(ents);
    if (!file_ent) {
        printf("File not found.\n");
        free(dup);
        fclose(img);
        return 1;
    }

    // Open destination
    FILE *out = fopen(out_path, "wb");
    if (!out) { perror("fopen"); free(dup); fclose(img); return 1; }

    // Copy through FAT chain
    uint32_t remaining = file_ent->file_size;
    uint32_t block     = file_ent->start_block;
    uint8_t *buf = malloc(sb.block_size);
    if (!buf) { perror("malloc"); return 1; }

    long fat_base = (long)sb.fat_start * sb.block_size;
    while (1) {
        fseek(img, (long)block * sb.block_size, SEEK_SET);
        size_t to_read = remaining < sb.block_size ? remaining : sb.block_size;
        fread(buf, 1, to_read, img);
        fwrite(buf, 1, to_read, out);
        remaining -= to_read;

        // next FAT entry
        uint32_t val_be;
        fseek(img, fat_base + block*4, SEEK_SET);
        fread(&val_be, 4, 1, img);
        uint32_t val = ntohl(val_be);
        if (val == 0xFFFFFFFF || remaining == 0) break;
        block = val;
    }

    free(buf);
    fclose(out);
    free(dup);
    fclose(img);
    return 0;
}
