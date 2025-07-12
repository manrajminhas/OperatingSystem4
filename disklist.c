// disklist.c
#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include "fs.h"

// Format raw 7-byte time into "YYYY/MM/DD hh:mm:ss"
static void format_time(uint8_t t[7], char out[20]) {
    uint16_t year = ntohs(*(uint16_t*)(t + 0));
    sprintf(out, "%04u/%02u/%02u %02u:%02u:%02u",
            year, t[2], t[3], t[4], t[5], t[6]);
}

// Given a path like "/", "/foo", or "/foo/bar", locate the
// directory's start_block & block_count. On success, return 0.
// On failure (not found), print message and return non-zero.
static int locate_dir(FILE *fp,
                      const superblock_t *sb,
                      const char *path,
                      uint32_t *out_start,
                      uint32_t *out_blocks)
{
    // Root special-case
    if (strcmp(path, "/") == 0) {
        *out_start  = sb->root_start;
        *out_blocks = sb->root_blocks;
        return 0;
    }

    // Duplicate & strip leading '/'
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
            // bit2 set => directory
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
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <image_file> <path>\n", argv[0]);
        return 1;
    }
    FILE *img = fopen(argv[1], "rb");
    if (!img) { perror("fopen"); return 1; }

    superblock_t sb;
    if (read_superblock(img, &sb) != 0) {
        fprintf(stderr, "Error reading superblock\n");
        fclose(img);
        return 1;
    }

    uint32_t dir_start, dir_blocks;
    if (locate_dir(img, &sb, argv[2], &dir_start, &dir_blocks) != 0) {
        fclose(img);
        return 1;
    }

    dir_entry_t *entries = NULL;
    int n = read_dir_entries(img, &sb, dir_start, dir_blocks, &entries);
    if (n < 0) {
        fprintf(stderr, "Error reading directory entries\n");
        fclose(img);
        return 1;
    }

    for (int i = 0; i < n; i++) {
        char ts[20];
        format_time(entries[i].ctime, ts);
        char type = (entries[i].status & 0x4) ? 'D' : 'F';
        printf("%c %10u %-30s %s\n",
               type,
               (unsigned)entries[i].file_size,
               entries[i].name,
               ts);
    }

    free(entries);
    fclose(img);
    return 0;
}
