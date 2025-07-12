#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <arpa/inet.h>
#include "fs.h"

// Get current local time into the 7-byte format
static void get_current_time(uint8_t t[7]) {
    time_t now = time(NULL);
    struct tm *lt = localtime(&now);
    uint16_t year = lt->tm_year + 1900;
    t[0] = (year >> 8) & 0xFF;
    t[1] = year & 0xFF;
    t[2] = lt->tm_mon + 1;
    t[3] = lt->tm_mday;
    t[4] = lt->tm_hour;
    t[5] = lt->tm_min;
    t[6] = lt->tm_sec;
}

// Same as disklist’s locate_dir, but silent on failure
static int find_dir(const superblock_t *sb, FILE *fp,
                    const char *path,
                    uint32_t *out_start, uint32_t *out_blocks)
{
    if (strcmp(path, "/") == 0) {
        *out_start  = sb->root_start;
        *out_blocks = sb->root_blocks;
        return 0;
    }
    char *tmp = strdup(path + 1);
    if (!tmp) return -1;

    uint32_t cur_start  = sb->root_start;
    uint32_t cur_blocks = sb->root_blocks;
    char *token = strtok(tmp, "/");
    while (token) {
        dir_entry_t *ents = NULL;
        int n = read_dir_entries(fp, sb, cur_start, cur_blocks, &ents);
        if (n < 0) { free(tmp); return -1; }
        int found = 0;
        for (int i = 0; i < n; i++) {
            if ((ents[i].status & 0x4) && strcmp(ents[i].name, token) == 0) {
                cur_start  = ents[i].start_block;
                cur_blocks = ents[i].block_count;
                found = 1;
                break;
            }
        }
        free(ents);
        if (!found) { free(tmp); return -1; }
        token = strtok(NULL, "/");
    }
    free(tmp);
    *out_start  = cur_start;
    *out_blocks = cur_blocks;
    return 0;
}

// Write a single directory entry into the first free slot
static int write_dir_entry(FILE *fp, const superblock_t *sb,
                           uint32_t dir_start, uint32_t dir_blocks,
                           const char *name, uint8_t status,
                           uint32_t start_block, uint32_t block_count,
                           uint32_t file_size)
{
    uint8_t ctime[7], mtime[7];
    get_current_time(ctime);
    memcpy(mtime, ctime, 7);

    long base = (long)dir_start * sb->block_size;
    long region = (long)dir_blocks * sb->block_size;

    for (long offset = 0; offset < region; offset += 64) {
        fseek(fp, base + offset, SEEK_SET);
        uint8_t st;
        fread(&st, 1, 1, fp);
        if ((st & 0x1) == 0) {
            uint8_t entry[64];
            memset(entry, 0xFF, 64);
            entry[0] = status;
            *(uint32_t*)(entry + 1) = htonl(start_block);
            *(uint32_t*)(entry + 5) = htonl(block_count);
            *(uint32_t*)(entry + 9) = htonl(file_size);
            memcpy(entry + 13, ctime, 7);
            memcpy(entry + 20, mtime, 7);
            size_t nlen = strlen(name);
            if (nlen > MAX_NAME_LEN) nlen = MAX_NAME_LEN;
            memcpy(entry + 27, name, nlen);
            entry[27 + nlen] = '\0';

            fseek(fp, base + offset, SEEK_SET);
            fwrite(entry, 1, 64, fp);
            return 0;
        }
    }
    return -1;
}

int main(int argc, char **argv) {
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <image> <host_src> <fs_dest>\n", argv[0]);
        return 1;
    }
    const char *img_path = argv[1];
    const char *src_path = argv[2];
    const char *fs_dest  = argv[3];

    // 1) Open host file
    FILE *src = fopen(src_path, "rb");
    if (!src) { printf("File not found.\n"); return 1; }
    fseek(src, 0, SEEK_END);
    uint32_t file_size = ftell(src);
    fseek(src, 0, SEEK_SET);

    // 2) Open image for update
    FILE *img = fopen(img_path, "r+b");
    if (!img) { perror("fopen"); fclose(src); return 1; }

    superblock_t sb;
    if (read_superblock(img, &sb) != 0) {
        fprintf(stderr, "Error reading superblock\n");
        fclose(src); fclose(img);
        return 1;
    }

    // 3) Split fs_dest into parent dir and filename
    char *dup = strdup(fs_dest);
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

    // 4) Locate or create parent directory
    uint32_t dir_start, dir_blocks;
    if (find_dir(&sb, img, dir_path, &dir_start, &dir_blocks) != 0) {
        // create directory under its parent
        char *pd = strdup(dir_path);
        char *ps = strrchr(pd + 1, '/');
        char *p_dir, *new_dir;
        if (!ps) {
            p_dir   = "/";
            new_dir = pd + 1;
        } else {
            *ps     = '\0';
            p_dir   = pd;
            new_dir = ps + 1;
        }
        uint32_t p_start, p_blocks;
        find_dir(&sb, img, p_dir, &p_start, &p_blocks);

        // allocate 1 block
        uint32_t total = sb.fat_blocks * (sb.block_size/4);
        long fat_base = (long)sb.fat_start * sb.block_size;
        uint32_t new_block = UINT32_MAX;
        for (uint32_t i = 0; i < total; i++) {
            fseek(img, fat_base + i*4, SEEK_SET);
            uint32_t v; fread(&v,4,1,img);
            if (ntohl(v) == 0) {
                new_block = i;
                uint32_t eof = htonl(0xFFFFFFFF);
                fseek(img, fat_base + i*4, SEEK_SET);
                fwrite(&eof,4,1,img);
                break;
            }
        }
        free(pd);
        if (new_block == UINT32_MAX) {
            fprintf(stderr, "Not enough space for directory\n");
            free(dup); fclose(src); fclose(img);
            return 1;
        }

        if (write_dir_entry(img, &sb, p_start, p_blocks,
                            new_dir, 0x1|0x4,
                            new_block, 1, 0) != 0)
        {
            fprintf(stderr, "Failed to create directory\n");
            free(dup); fclose(src); fclose(img);
            return 1;
        }
        dir_start  = new_block;
        dir_blocks = 1;
    }

    // 5) Allocate blocks for the file
    uint32_t blocks_needed = (file_size + sb.block_size - 1) / sb.block_size;
    uint32_t total_entries = sb.fat_blocks * (sb.block_size/4);
    long fat_base = (long)sb.fat_start * sb.block_size;

    uint32_t *chain = malloc(blocks_needed * sizeof(uint32_t));
    int found = 0;
    for (uint32_t i = 0; i < total_entries && found < (int)blocks_needed; i++) {
        fseek(img, fat_base + i*4, SEEK_SET);
        uint32_t v; fread(&v,4,1,img);
        if (ntohl(v) == 0) {
            chain[found++] = i;
        }
    }
    if (found < (int)blocks_needed) {
        fprintf(stderr, "Not enough space for file\n");
        free(chain); free(dup); fclose(src); fclose(img);
        return 1;
    }

    // 6) Link them in the FAT
    for (uint32_t idx = 0; idx + 1 < (uint32_t)blocks_needed; idx++) {
        uint32_t next = htonl(chain[idx+1]);
        fseek(img, fat_base + chain[idx]*4, SEEK_SET);
        fwrite(&next,4,1,img);
    }
    uint32_t eof = htonl(0xFFFFFFFF);
    fseek(img, fat_base + chain[blocks_needed-1]*4, SEEK_SET);
    fwrite(&eof,4,1,img);

    // 7) Write file data into each block
    uint8_t *buf = malloc(sb.block_size);
    for (uint32_t idx = 0; idx < blocks_needed; idx++) {
        fseek(img, (long)chain[idx] * sb.block_size, SEEK_SET);
        size_t chunk = sb.block_size;
        if (idx == blocks_needed - 1)
            chunk = file_size - (blocks_needed - 1) * sb.block_size;
        fread(buf, 1, chunk, src);
        fwrite(buf, 1, chunk, img);
    }
    free(buf);
    fclose(src);

    // 8) Add directory entry for the file
    if (write_dir_entry(img, &sb, dir_start, dir_blocks,
                        file_name, 0x1|0x2,
                        chain[0], blocks_needed, file_size) != 0)
    {
        fprintf(stderr, "Failed to write file entry\n");
        free(chain); free(dup); fclose(img);
        return 1;
    }

    free(chain);
    free(dup);
    fclose(img);
    return 0;
}
