#ifndef FS_H
#define FS_H

#include <stdint.h>
#include <stdio.h>

#define FS_ID_LEN     8
#define MAX_NAME_LEN 30

// --- Superblock structure ---
typedef struct {
    char     fs_id[FS_ID_LEN + 1]; // "CSC360FS" + '\0'
    uint16_t block_size;           // bytes
    uint32_t block_count;
    uint32_t fat_start;            // block index
    uint32_t fat_blocks;
    uint32_t root_start;
    uint32_t root_blocks;
} superblock_t;

// Read/inspect superblock and FAT
int read_superblock(FILE *fp, superblock_t *sb);
int analyze_fat(FILE *fp,
                const superblock_t *sb,
                uint32_t *free_cnt,
                uint32_t *reserved_cnt,
                uint32_t *alloc_cnt);

// --- Directory‐entry structure (64 bytes on‐disk) ---
typedef struct {
    uint8_t  status;               // bit0=in‐use, bit1=file, bit2=dir
    uint32_t start_block;          // big‐endian on‐disk
    uint32_t block_count;
    uint32_t file_size;
    uint8_t  ctime[7];             // YYYY MM DD hh mm ss
    uint8_t  mtime[7];
    char     name[MAX_NAME_LEN+1]; // null-terminated
} dir_entry_t;

// Read all in‐use entries in a directory region
// Caller must free *out_entries.
// Returns number of entries or -1 on error.
int read_dir_entries(FILE *fp,
                     const superblock_t *sb,
                     uint32_t dir_start,
                     uint32_t dir_blocks,
                     dir_entry_t **out_entries);

#endif // FS_H
