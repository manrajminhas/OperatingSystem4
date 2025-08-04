# OperatingSystem4

A collection of C utilities for inspecting and manipulating a simple FAT‑like file system image, this personal project includes four command‑line tools:

* **diskinfo**: Display superblock and FAT statistics.
* **disklist**: List directory contents in the image.
* **diskget**: Extract a file from the image to the host.
* **diskput**: Insert a host file into the image, creating directories as needed.

## Repository Structure

```
├── Makefile             # Builds all four executables
├── fs.h/fs.c            # Shared file system parsing routines
├── diskinfo.c           # Part I: superblock/FAT inspector
├── disklist.c           # Part II: directory lister
├── diskget.c            # Part III: file extractor
├── diskput.c            # Part IV: file inserter
├── test.img             # Sample empty file system image
├── non-empty.img        # Sample image with subdirectories
└── README.md            # This file
```

## Requirements

* GCC (or compatible C compiler)
* POSIX‑compatible environment (Linux recommended)
* `make` utility

## Build Instructions

Clone or download the repo, then:

```bash
make clean
make
```

This produces four executables: `diskinfo`, `disklist`, `diskget`, and `diskput`.

## Usage

### diskinfo

Inspect superblock and FAT summary:

```bash
./diskinfo <image-file>
```

### disklist

List entries in a directory:

```bash
./disklist <image-file> <path>
```

e.g. `./disklist test.img /` or `./disklist non-empty.img /sub_Dir`

### diskget

Extract a file from the image:

```bash
./diskget <image-file> <fs-path> <host-dest>
```

e.g. `./diskget non-empty.img /test.txt local_copy.txt`

### diskput

Insert a host file into the image (creates directory if needed):

```bash
./diskput <image-file> <host-src> <fs-dest>
```

e.g. `./diskput test.img myfile.txt /docs/myfile.txt`

## File System Specification

The image format is a simplified FAT‑like layout:

1. **Superblock** (first 512 bytes) with block size, counts, and offsets.
2. **File Allocation Table (FAT)**: 4‑byte entries linking data blocks; `0xFFFFFFFF` marks end‑of‑file.
3. **Directory entries**: 64‑byte records (status, start block, size, timestamps, name).

Refer to the source code comments in `fs.h` and the assignment prompt for full details.

## Testing

Sample images (`test.img` and `non-empty.img`) are provided. Use the commands above to verify functionality. Example:

```bash
./disklist test.img /
./diskput test.img hello.txt /hello.txt
./diskget test.img /hello.txt hello_copy.txt
```
