#ifndef FS_H
#define FS_H

#include <stdint.h>
#include <stdio.h>

#define FS_ID_LEN 8

typedef struct {
    char     fs_id[FS_ID_LEN + 1]; // "CSC360FS" + '\0'
    uint16_t block_size;           // in bytes
    uint32_t block_count;
    uint32_t fat_start;            // block index
    uint32_t fat_blocks;
    uint32_t root_start;
    uint32_t root_blocks;
} superblock_t;

// Read the superblock from 'fp' (must be opened in "rb").
// Returns 0 on success, non-zero on failure.
int read_superblock(FILE *fp, superblock_t *sb);

// Count FAT entries by category; all offsets are in blocks.
// Returns 0 on success.
int analyze_fat(FILE *fp,
                const superblock_t *sb,
                uint32_t *free_cnt,
                uint32_t *reserved_cnt,
                uint32_t *alloc_cnt);

#endif // FS_H
