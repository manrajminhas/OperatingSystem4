#include "fs.h"
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>

// --- Superblock reader ---
int read_superblock(FILE *fp, superblock_t *sb) {
    if (fseek(fp, 0, SEEK_SET) != 0) return -1;
    if (fread(sb->fs_id, 1, FS_ID_LEN, fp) != FS_ID_LEN) return -1;
    sb->fs_id[FS_ID_LEN] = '\0';

    uint16_t be16;
    uint32_t be32;
    if (fread(&be16, 2, 1, fp) != 1) return -1;
    sb->block_size = ntohs(be16);

    if (fread(&be32, 4, 1, fp) != 1) return -1;
    sb->block_count = ntohl(be32);

    if (fread(&be32, 4, 1, fp) != 1) return -1;
    sb->fat_start = ntohl(be32);

    if (fread(&be32, 4, 1, fp) != 1) return -1;
    sb->fat_blocks = ntohl(be32);

    if (fread(&be32, 4, 1, fp) != 1) return -1;
    sb->root_start = ntohl(be32);

    if (fread(&be32, 4, 1, fp) != 1) return -1;
    sb->root_blocks = ntohl(be32);

    return 0;
}

// --- FAT analyzer ---
int analyze_fat(FILE *fp,
                const superblock_t *sb,
                uint32_t *free_cnt,
                uint32_t *reserved_cnt,
                uint32_t *alloc_cnt)
{
    *free_cnt = *reserved_cnt = *alloc_cnt = 0;
    uint32_t entries_per_block = sb->block_size / 4;
    uint32_t total_entries     = entries_per_block * sb->fat_blocks;

    if (fseek(fp, (long)sb->fat_start * sb->block_size, SEEK_SET) != 0)
        return -1;

    for (uint32_t i = 0; i < total_entries; i++) {
        uint32_t val_be;
        if (fread(&val_be, 4, 1, fp) != 1) return -1;
        uint32_t val = ntohl(val_be);
        if (val == 0x00000000)
            (*free_cnt)++;
        else if (val == 0x00000001)
            (*reserved_cnt)++;
        else
            (*alloc_cnt)++;
    }
    return 0;
}

// --- Directory‐entry reader ---
int read_dir_entries(FILE *fp,
                     const superblock_t *sb,
                     uint32_t dir_start,
                     uint32_t dir_blocks,
                     dir_entry_t **out_entries)
{
    size_t buf_size = (size_t)sb->block_size * dir_blocks;
    uint8_t *buf = malloc(buf_size);
    if (!buf) return -1;

    if (fseek(fp, (long)dir_start * sb->block_size, SEEK_SET) != 0 ||
        fread(buf, 1, buf_size, fp) != buf_size)
    {
        free(buf);
        return -1;
    }

    int max_entries = buf_size / 64;
    dir_entry_t *entries = calloc(max_entries, sizeof(dir_entry_t));
    if (!entries) { free(buf); return -1; }

    int count = 0;
    for (int i = 0; i < max_entries; i++) {
        uint8_t *e = buf + i*64;
        if (e[0] & 0x1) {  // in‐use
            entries[count].status      = e[0];
            entries[count].start_block = ntohl(*(uint32_t*)(e + 1));
            entries[count].block_count = ntohl(*(uint32_t*)(e + 5));
            entries[count].file_size   = ntohl(*(uint32_t*)(e + 9));
            memcpy(entries[count].ctime, e + 13, 7);
            memcpy(entries[count].mtime, e + 20, 7);
            memcpy(entries[count].name,  e + 27, MAX_NAME_LEN);
            entries[count].name[MAX_NAME_LEN] = '\0';
            count++;
        }
    }

    free(buf);
    *out_entries = entries;
    return count;
}
