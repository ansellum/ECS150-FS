#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "disk.h"
#include "fs.h"

#define FS_NUM_OF_FILES 128

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

struct __attribute__((__packed__)) superblock {
	char sig[8];			// signature: ECS150FS 

	uint16_t totalNumOfBlocks;	// Total amount of blocks of virtual disk
	uint16_t indexOfRootDir;	// Root directory block index
	uint16_t indexOfDataBlocks;	// Data block start index
	uint16_t numOfDataBlocks;	// Data block start index

	uint8_t  numOfFatBlocks;	// Number of blocks for FAT
	uint8_t  unused[4079];		// Padding
};

/*
* The FAT is a flat array, possibly spanning several blocks,
* which entries are composed of 16-bit unsigned words.
* There are as many entries as data blocks in the disk.
* Each entry in the FAT is 16-bit wide.
*
* FAT index:	0	1	2	3	4	5	6	7	8	9	10	…
* Content:	0xFFFF	8	3	4	5	6	0xFFFF	0	0xFFFF	0	0	…
*/
struct __attribute__((__packed__)) fat {
	uint16_t* entry; // 16-bit entries; # of entries determined at runtime
};


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
struct __attribute__((__packed__)) rootDir {
	uint32_t file[FS_NUM_OF_FILES]; // Each file entry has the above layout
};

int fs_mount(const char *diskname)
{
	/* TODO: Phase 1 */
}

int fs_umount(void)
{
	/* TODO: Phase 1 */
}

int fs_info(void)
{
	/* TODO: Phase 1 */
}

int fs_create(const char *filename)
{
	/* TODO: Phase 2 */
}

int fs_delete(const char *filename)
{
	/* TODO: Phase 2 */
}

int fs_ls(void)
{
	/* TODO: Phase 2 */
}

int fs_open(const char *filename)
{
	/* TODO: Phase 3 */
}

int fs_close(int fd)
{
	/* TODO: Phase 3 */
}

int fs_stat(int fd)
{
	/* TODO: Phase 3 */
}

int fs_lseek(int fd, size_t offset)
{
	/* TODO: Phase 3 */
}

int fs_write(int fd, void *buf, size_t count)
{
	/* TODO: Phase 4 */
}

int fs_read(int fd, void *buf, size_t count)
{
	/* TODO: Phase 4 */
}

