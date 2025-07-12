#include <stdio.h>
#include "fs.h"

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <image_file>\n", argv[0]);
        return 1;
    }
    FILE *img = fopen(argv[1], "rb");
    if (!img) {
        perror("fopen");
        return 1;
    }

    superblock_t sb;
    if (read_superblock(img, &sb) != 0) {
        fprintf(stderr, "Failed to read superblock\n");
        return 1;
    }

    printf("Super block information:\n");
    printf("Block size: %u\n", sb.block_size);
    printf("Block count: %u\n", sb.block_count);
    printf("FAT starts: %u\n", sb.fat_start);
    printf("FAT blocks: %u\n", sb.fat_blocks);
    printf("Root directory start: %u\n", sb.root_start);
    printf("Root directory blocks: %u\n\n", sb.root_blocks);

    uint32_t free_b, res_b, alloc_b;
    if (analyze_fat(img, &sb, &free_b, &res_b, &alloc_b) != 0) {
        fprintf(stderr, "Failed to analyze FAT\n");
        return 1;
    }

    printf("FAT information:\n");
    printf("Free Blocks: %u\n", free_b);
    printf("Reserved Blocks: %u\n", res_b);
    printf("Allocated Blocks: %u\n", alloc_b);

    fclose(img);
    return 0;
}
