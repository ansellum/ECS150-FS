#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "disk.h"
#include "fs.h"

#define fs_error(fmt, ...) \
	fprintf(stderr, "%s: "fmt"\n", __func__, ##__VA_ARGS__)

#define UNUSED(x) (void)(x)

#define FS_FAT_ENTRY_MAX_COUNT 2048

/* Global Variables*/
// These are pointers to the byte addresses within the disk file
struct superblock* superblock;
struct FATEntry* FAT[];
struct rootDir* root_dir;

/* Data Structures */

/* 
* The superblock is the first block of the file system.
* Its internal format is:
*
* Offset	Length (bytes)	Description
* 
* 0x00		8		Signature (must be equal to “ECS150FS”)
* 0x08		2		Total amount of blocks of virtual disk
* 0x0A		2		Root directory block index
* 0x0C		2		Data block start index
* 0x0E		2		Amount of data blocks
* 0x10		1		Number of blocks for FAT
* 0x11		4079		Unused/Padding
*/
struct superblock {
	char sig[8];			// signature: ECS150FS 

	uint16_t totalNumOfBlocks;	// Total amount of blocks of virtual disk
	uint16_t indexOfRootDir;	// Root directory block index
	uint16_t indexOfDataBlocks;	// Data block start index
	uint16_t numOfDataBlocks;	// Data block start index

	uint8_t  numOfFatBlocks;	// Number of blocks for FAT
	uint8_t  unused[4079];		// Padding
}__attribute__((__packed__));

/*
* The FAT is a flat array, possibly spanning several blocks,
* which entries are composed of 16-bit unsigned words.
* There are as many entries as data blocks in the disk.
* Each entry in the FAT is 16-bit wide.
*
* FAT index:	0	1	2	3	4	5	6	7	8	9	10	…
* Content:	0xFFFF	8	3	4	5	6	0xFFFF	0	0xFFFF	0	0	…
*/
struct FATEntry {
	uint16_t entry;
}__attribute__((__packed__));

/*
* The root directory is an array of 128 entries stored in the block following the FAT. 
* Each entry is 32-byte wide and describes a file, according to the following format:
*
* Offset	Length (bytes)	Description
* 
* 0x00		16		Filename (including NULL character)
* 0x10		4		Size of the file (in bytes)
* 0x14		2		Index of the first data block
* 0x16		10		Unused/Padding
*/
struct fileEntry {
	char fileName[FS_FILENAME_LEN];

	uint32_t fileSize;         // Length 4 bytes file size
	uint8_t  indexOfDataBlock; // Index of the first data block
	uint8_t  unused[10];
} __attribute__((__packed__));

struct rootDir {
	struct fileEntry file[FS_FILE_MAX_COUNT]; // Each file entry has the above layout, which will be defined later
}__attribute__((__packed__));

/* Filesystem Functions */
int fs_mount(const char *diskname)
{
	/* Mount disk */

	// Open file
	if (block_disk_open(diskname) < 0) {
		fs_error("Couldn't open disk");
		return -1;
	}

	// Read in superblock
	if (block_read(0, superblock) < 0) {
		fs_error("Couldn't read superblock");
		return -1;
	}

	// Read in root directory
	if (block_read(superblock->indexOfRootDir, root_dir) < 0) {
		fs_error("Couldn't read root directory");
		return -1;
	}

	// Iterate through FAT blocks
	FAT = malloc(superblock->numOfDataBlocks * sizeof(uint16_t));
	for (int i = 0; i < superblock->numOfFatBlocks; ++i) {
		// Read FAT block into entry address
		if (block_read(i + 1, FAT[i * FS_FAT_ENTRY_MAX_COUNT]) < 0) {
			fs_error("Couldn't read FAT");
			return -1;
		}
	}

	/* Error checking */

	// Check signature
	if (strcmp(superblock->sig, "ECS150FS") != 0) {
		fs_error("Filesystem has an invalid format");
		return -1;
	}

	// Check disk size
	if (superblock->totalNumOfBlocks != block_disk_count()) {
		fs_error("Mismatched number of blocks");
		return -1;
	}

	return 0;
}

int fs_umount(void)
{
	/* TODO: Phase 1 */
	return 0;
}

int fs_info(void)
{
	/* TODO: Phase 1 */
	return 0;
}

int fs_create(const char *filename)
{
	UNUSED(filename);
	/* TODO: Phase 2 */
	return 0;
}

int fs_delete(const char *filename)
{
	UNUSED(filename);
	/* TODO: Phase 2 */
	return 0;
}

int fs_ls(void)
{
	/* TODO: Phase 2 */
	return 0;
}

int fs_open(const char *filename)
{
	UNUSED(filename);
	/* TODO: Phase 3 */
	return 0;
}

int fs_close(int fd)
{
	UNUSED(fd);
	/* TODO: Phase 3 */
	return 0;
}

int fs_stat(int fd)
{
	UNUSED(fd);
	/* TODO: Phase 3 */
	return 0;
}

int fs_lseek(int fd, size_t offset)
{
	UNUSED(fd);
	UNUSED(offset);
	/* TODO: Phase 3 */
	return 0;
}

int fs_write(int fd, void *buf, size_t count)
{
	UNUSED(fd);
	UNUSED(buf);
	UNUSED(count);
	/* TODO: Phase 4 */
	return 0;
}

int fs_read(int fd, void *buf, size_t count)
{
	UNUSED(fd);
	UNUSED(buf);
	UNUSED(count);
	/* TODO: Phase 4 */
	return 0;
}

