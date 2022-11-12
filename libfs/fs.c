#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "disk.h"
#include "fs.h"

#define fs_error(...)				\
{							\
	fprintf(stderr, "%s: "fmt"\n", __func__, ##__VA_ARGS__)	\
	return -1;					\
}

#define fs_perror(...)				\
{							\
	perror(__VA_ARGS__);	\
	return -1;					\
}

#define UNUSED(x) (void)(x)

#define FS_FAT_ENTRY_MAX_COUNT (BLOCK_SIZE/2)
#define SIGNATURE 0x5346303531534345	// 'ECS150FS' in little-endian
#define FAT_EOC 0xFFFF

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
	uint64_t sig;			// signature: ECS150FS 
	uint16_t total_blk_count;	// Total amount of blocks of virtual disk
	uint16_t rdir_blk;		// Root directory block index
	uint16_t data_blk;		// Data block start index
	uint16_t data_blk_count;	// Data block start index
	uint8_t  fat_blk_count;		// Number of blocks for FAT
	uint8_t  unused[4079];		// Padding
}__attribute__((packed));

/*
* The FAT is a flat array, possibly spanning several blocks,
* which entries are composed of 16-bit unsigned words.
* There are as many entries as data blocks in the disk.
* Each entry in the FAT is 16-bit wide.
*
* FAT index:	0	1	2	3	4	5	6	7	8	9	10	…
* Content:	0xFFFF	8	3	4	5	6	0xFFFF	0	0xFFFF	0	0	…
*/
struct FAT {
	uint16_t entry[4 * FS_FAT_ENTRY_MAX_COUNT]; // Maximum of 8192 data blocks => Maximum of 4 FAT blocks
}__attribute__((packed));

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
struct file_entry {
	uint8_t  file_name[FS_FILENAME_LEN];
	uint32_t file_size;		// Length 4 bytes file size
	uint16_t data_blk;	// Index of the first data block
	uint8_t  unused[10];
} __attribute__((packed));

struct root_dir {
	struct file_entry file[FS_FILE_MAX_COUNT]; // Each file entry has the above layout, which will be defined later
}__attribute__((packed));

/* Global Variables*/
struct superblock superblock;
struct FAT FAT; // Maximum of 4 FAT blocks, 2048 entires each
struct root_dir root_dir;
uint8_t open_fd = FS_OPEN_MAX_COUNT;

/* Filesystem Functions */
int fs_mount(const char *diskname)
{
	/* Mount disk */
	// Open file
	if (block_disk_open(diskname) < 0)
		fs_error("Couldn't open disk");

	// Read in superblock
	if (block_read(0, &superblock) < 0)
		fs_error("Couldn't read superblock");

	// Read in root directory
	if (block_read(superblock.rdir_blk, &root_dir) < 0)
		fs_error("Couldn't read root directory");

	// Read FAT by iterating at a block-level
	for (int i = 0; i < superblock.fat_blk_count; ++i) {
		// Find the correct FAT block & pass the corresponding entry address as the buffer
		if (block_read(i + 1, &(FAT.entry[i * FS_FAT_ENTRY_MAX_COUNT])) < 0)
			fs_error("Couldn't read FAT")
	}

	/* Error checking */
	// Check signature
	if (superblock.sig != SIGNATURE)
		fs_error("Filesystem has an invalid format");

	// Check disk size
	if (superblock.total_blk_count != block_disk_count())
		fs_error("Mismatched number of total blocks");

	return 0;
}

int fs_umount(void)
{
	/* Write back blocks */
	// Root Directory
	if (block_write(superblock.rdir_blk, &root_dir) < 0)
		fs_error("Couldn't write over root directory");

	// FAT
	for (int i = 0; i < superblock.fat_blk_count; ++i) {
		// Find the correct FAT block & pass the corresponding entry address as the buffer
		if (block_write(i + 1, &(FAT.entry[i * FS_FAT_ENTRY_MAX_COUNT])) < 0)
			fs_error("Couldn't write over FAT");
	}

	/* Empty all structs */
	superblock = (const struct superblock){ 0 };
	FAT = (const struct FAT){ 0 };
	root_dir = (const struct root_dir){ 0 };

	// Also write back data blocks???
	// Could be done w/ block_write within fs_write

	return 0;
}

int fs_info(void)
{
	// Root Directory Traversal
	int free_file_count = FS_FILE_MAX_COUNT;
	for (int i = 0; i < FS_FILE_MAX_COUNT; ++i) {
		if (root_dir.file[i].file_name[0] == '\0')
			continue;
		free_file_count--;
	}

	// FAT Traversal
	int free_data_blk_count = superblock.data_blk_count;
	for (int i = 0; i < superblock.data_blk_count; ++i) {
		if (FAT.entry[i] == '\0')
			continue;
		free_data_blk_count--;
	}

	fprintf(stdout, "FS Info:\n");
	fprintf(stdout, "total_blk_count=%d\n",		superblock.total_blk_count);
	fprintf(stdout, "fat_blk_count=%d\n",		superblock.fat_blk_count);
	fprintf(stdout, "rdir_blk=%d\n",		superblock.rdir_blk);
	fprintf(stdout, "data_blk=%d\n",		superblock.data_blk);
	fprintf(stdout, "data_blk_count=%d\n",		superblock.data_blk_count);
	fprintf(stdout, "fat_free_ratio=%d/%d\n",	free_data_blk_count,	superblock.data_blk_count); // Not functional yet
	fprintf(stdout, "rdir_free_ratio=%d/%d\n",	free_file_count,	FS_FILE_MAX_COUNT);

	return 0;
}

int fs_create(const char *filename)
{
	/* Error Checking */
	// Check if FS is mounted
	if (superblock.sig != SIGNATURE)
		fs_error("Filesystem not mounted");

	//Check if filename is NULL or empty
	if (filename == NULL || filename[0] == '\0')
		fs_error("Filename is invalid (either NULL or empty)");

	// Check filename length (strlen doesn't count NULL, therefore use >=)
	if (strlen(filename) >= FS_FILENAME_LEN)
		fs_error("Filename must be less than 16 characters");

	/* Find empty root entry */
	int free_index = 0;
	for (; free_index < FS_FILE_MAX_COUNT; free_index++) {
		// Break loop if empty entry is found
		if (root_dir.file[free_index].file_name[0] == '\0')
			break;

		// Check if file already exists
		if (strcmp(filename, (char*) root_dir.file[free_index].file_name) == 0)
			fs_error("File already exists");
	}

	// Check root directory capacity
	if (free_index == FS_FILE_MAX_COUNT)
		fs_error("Filesystem is full");

	/* Create file */
	strcpy((char*)root_dir.file[free_index].file_name, filename);
	root_dir.file[free_index].file_size = 0;
	root_dir.file[free_index].data_blk = FAT_EOC;

	return 0;
}

int fs_delete(const char *filename)
{
	/* Error Checking */
	// Check if FS is mounted
	if (superblock.sig != SIGNATURE)
		fs_error("Filesystem not mounted");

	//Check if filename is NULL or empty
	if (filename == NULL || filename[0] == '\0')
		fs_error("Filename is invalid (either NULL or empty)");

	/* TODO: CHECK IF FILE IS OPEN */

	/* Find File in Root Directory */
	int death_index = 0;
	for (; death_index < FS_FILE_MAX_COUNT; death_index++) {
		// Break loop if entry is found
		if (strcmp((char*)root_dir.file[death_index].file_name, filename) == 0)
			break;
	}
		
	// Check if file was found
	if (death_index == FS_FILE_MAX_COUNT)
		fs_error("File not found");

	/* Delete File */
	root_dir.file[death_index].file_name[0] = '\0';

	return 0;
}

int fs_ls(void)
{
	/* Error Checking */
	// Check if FS is mounted
	if (superblock.sig != SIGNATURE)
		fs_error("Filesystem not mounted");

	/* List files */
	fprintf(stdout, "FS Ls:\n");
	for (int index = 0; index < FS_FILE_MAX_COUNT; index++) {
		//Skip index if empty
		if (root_dir.file[index].file_name[0] == '\0')
			continue;

		fprintf(stdout, "file: %s, size: %d, data_blk: %d\n", 
			root_dir.file[index].file_name, root_dir.file[index].file_size, root_dir.file[index].data_blk);
	}

	return 0;
}

int fs_open(const char *filename)
{
	int fd;

	/* Error Checking */
	// Check if FS is mounted
	if (superblock.sig != SIGNATURE)
		fs_error("Filesystem not mounted");

	// Check if filename is NULL or empty
	if (filename == NULL || filename[0] == '\0')
		fs_error("Filename is invalid (either NULL or empty)");

	// Check if the maximum for open files has been reached
	if (open_fd == 0)
		fs_error("Too many files are currently open");

	/* Open File */
	if ((fd = open(filename, O_RDWR)) < 0)
		fs_error("File does not exist");

	open_fd--;
	return fd;
}

int fs_close(int fd)
{
	/* Error Checking */
	// Check if FS is mounted
	if (superblock.sig != SIGNATURE)
		fs_error("Filesystem not mounted");

	if (close(fd) < 0)
		fs_error("File descriptor is invalid");

	return 0;
}

int fs_stat(int fd)
{
	struct stat* info;

	/* Error Checking */
	// Check if FS is mounted
	if (superblock.sig != SIGNATURE)
		fs_error("Filesystem not mounted");

	if (stat(fd, info) < 0)
		fs_error("File descriptor is invalid");

	if ((info = malloc(sizeof(struct stat))) < 0)
		fs_perror("malloc");
		
	return info->st_size;
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

