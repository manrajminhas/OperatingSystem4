#include "fs.h"
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>  // for ntohl, ntohs

int read_superblock(FILE *fp, superblock_t *sb) {
    if (fseek(fp, 0, SEEK_SET) != 0) return -1;
    // read FS identifier
    if (fread(sb->fs_id, 1, FS_ID_LEN, fp) != FS_ID_LEN) return -1;
    sb->fs_id[FS_ID_LEN] = '\0';
    // read and convert
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

int analyze_fat(FILE *fp,
                const superblock_t *sb,
                uint32_t *free_cnt,
                uint32_t *reserved_cnt,
                uint32_t *alloc_cnt)
{
    *free_cnt = *reserved_cnt = *alloc_cnt = 0;
    uint32_t entries_per_block = sb->block_size / 4;
    uint32_t total_entries = entries_per_block * sb->fat_blocks;
    uint32_t i, val;
    // Seek to FAT start in bytes:
    if (fseek(fp, (long)sb->fat_start * sb->block_size, SEEK_SET) != 0)
        return -1;

    for (i = 0; i < total_entries; i++) {
        if (fread(&val, 4, 1, fp) != 1) return -1;
        val = ntohl(val);
        if (val == 0x00000000)        (*free_cnt)++;
        else if (val == 0x00000001)   (*reserved_cnt)++;
        else if (val == 0xFFFFFFFF)   (*alloc_cnt)++;  // last block
        else                          (*alloc_cnt)++;
    }
    return 0;
}
