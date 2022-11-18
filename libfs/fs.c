#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "disk.h"
#include "fs.h"

#define error(fmt, ...) \
	fprintf(stderr, "%s: "fmt"\n", __func__, ##__VA_ARGS__)

#define fs_error(...)				\
{							\
	error(__VA_ARGS__);	\
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

/**
* The superblock is the first block of the file system.
* See HTML doc for format specifications.
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

/**
* The FAT is a flat array, possibly spanning several blocks, which entries are composed of 16-bit unsigned words.
* Empty entries are marked by a '0'; non-zero entries are part of a chainmap representing the next block in the chainmap
* See HTML doc for format specifications.
*/
uint16_t FAT[4 * FS_FAT_ENTRY_MAX_COUNT]; // Maximum of 4 FAT blocks, 2048 entires each

/**
* The root directory is an array of 128 entries that describe the filesystem's contained files.
* See HTML doc for format specifications
*/
struct file_entry { // 32B
	uint8_t  file_name[FS_FILENAME_LEN];
	uint32_t file_size;		// Length 4 bytes file size
	uint16_t data_blk;		// Index of the first data block
	uint8_t  unused[10];
}__attribute__((packed));

struct root_dir {
	struct file_entry file[FS_FILE_MAX_COUNT]; // Each file entry has the above layout, which will be defined later
}__attribute__((packed));

/**
* A data block is byte-addressable storage block with the capacity of 4096 bytes. This is mimicked by using an array of 
* 4096 uint8_t's, making each byte readable.
*/

struct data_block {
	uint8_t byte[BLOCK_SIZE];
};

/**
* A file descriptor is obtained using fs_open() and can support multiple operations (reading, writing, changing the file offset, etc).
* The library must support a maximum of 32 file descriptors that can be open simultaneously.
* A file descriptor is associated to a file and also contains a file offset.
*/
struct file_descriptor {
	struct file_entry* entry;
	size_t  offset;
};

/* Global Variables*/
struct superblock superblock;
struct root_dir root_dir;
struct data_block bounce;
struct file_descriptor fd_list[FS_OPEN_MAX_COUNT];

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
		if (block_read(i + 1, &(FAT[i * FS_FAT_ENTRY_MAX_COUNT])) < 0)
			fs_error("Couldn't read FAT")
	}

	/* Error checking */
	// Check signature
	if (superblock.sig != SIGNATURE)
		fs_error("Filesystem has an invalid format");

	// Check disk size
	if (superblock.total_blk_count != block_disk_count())
		fs_error("Mismatched number of total blocks");

	/* Prepare file descriptors */
	for (int i = 0; i < FS_OPEN_MAX_COUNT; ++i) {
		fd_list[i].entry = NULL;
		fd_list[i].offset = 0;
	}

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
		if (block_write(i + 1, &(FAT[i * FS_FAT_ENTRY_MAX_COUNT])) < 0)
			fs_error("Couldn't write over FAT");
	}

	/* Empty all structs */
	superblock = (const struct superblock){ 0 };
	memset(FAT, 0, sizeof(FAT));
	root_dir = (const struct root_dir){ 0 };

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
		if (FAT[i] == '\0')
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
	/* Error Checking */
	// Check if FS is mounted
	if (superblock.sig != SIGNATURE)
		fs_error("Filesystem not mounted");

	// Check if filename is NULL or empty
	if (filename == NULL || filename[0] == '\0')
		fs_error("Filename is invalid (either NULL or empty)");

	/* Find file in root directory */
	int file_root_index = 0;
	for (; file_root_index < FS_FILE_MAX_COUNT; file_root_index++) {
		// Break loop if empty entry is found
		if (strcmp((char*)root_dir.file[file_root_index].file_name, filename) == 0)
			break;
	}
	if (file_root_index == FS_FILE_MAX_COUNT)
		fs_error("No such file or directory");

	/* Look for empty file descriptor */
	int free_fd = 0;
	for (; free_fd < FS_OPEN_MAX_COUNT; ++free_fd) {
		if (fd_list[free_fd].entry == NULL)
			break;
	}
	if (free_fd == FS_OPEN_MAX_COUNT)
		fs_error("Too many files are currently open");

	/* Assign file to fd */
	fd_list[free_fd].entry = &(root_dir.file[file_root_index]);

	return free_fd;
}

int fs_close(int fd)
{
	/* Error Checking */
	// Check if FS is mounted
	if (superblock.sig != SIGNATURE)
		fs_error("Filesystem not mounted");

	// Check if file descriptor is closed or out of bounds
	if (fd_list[fd].entry == NULL || fd >= FS_OPEN_MAX_COUNT)
		fs_error("Invalid file descriptor");

	/* Close the file (i.e. reset file descriptor) */
	fd_list[fd].entry = NULL;
	fd_list[fd].offset = 0;

	return 0;
}

int fs_stat(int fd)
{
	/* Error Checking */
	// Check if FS is mounted
	if (superblock.sig != SIGNATURE)
		fs_error("Filesystem not mounted");

	// Check if file descriptor is open (i.e. not used) or out of bounds
	if (fd_list[fd].entry == NULL || fd >= FS_OPEN_MAX_COUNT)
		fs_error("Invalid file descriptor");

	return fd_list[fd].entry->file_size;
}

int fs_lseek(int fd, size_t offset)
{
	// Fetches file size AND does error checks
	int file_size = fs_stat(fd);
	if (file_size < 0)
		fs_error("fs_stat");

	// Check if offset exceeds filesize
	if ((size_t)file_size < offset)
		fs_error("Requested offset surpasses file boundaries");

	/* Perform lseek */
	fd_list[fd].offset = offset;

	return 0;
}

int fs_write(int fd, void *buf, size_t count)
{
	UNUSED(count);

	/* Error Checking */
	// Check if FS is mounted
	if (superblock.sig != SIGNATURE)
		fs_error("Filesystem not mounted");

	// Check if file descriptor is closed or out of bounds
	if (fd_list[fd].entry == NULL || fd >= FS_OPEN_MAX_COUNT)
		fs_error("Invalid file descriptor");

	if (buf == NULL)
		fs_error("buf is NULL");

	/* Begin Write */

	return 0;
}

/*
* get_offset_data_blk - Retrieve offset'd data block
* @offset: Offset
* 
* An existing offset (from a previous read or lseek) may push beyond 4096, thus spanning into a second data block.
* This function should retrieve the correct data block to continue reading from.
* 
* Return: pointer to correct data block if successful, NULL otherwise 
*/
uint16_t fetch_offset_block_index(int fd)
{
	/* Block Offset */
	// Get the number of blocks from the first data block where the offset is
	uint16_t block_offset = 0;
	for (uint32_t i = 0; i < fd_list[fd].offset; ++i) {
		// If i is divisible by 4096, it is a new block.
		if (i % BLOCK_SIZE || i == 0)
			continue;

		block_offset++;
	}

	/* Find the block in FAT to access */
	// Begin with open file's first data block
	uint16_t access_point = fd_list[fd].entry->data_blk;
	for (uint16_t i = 0; i < block_offset; ++i) {
		// Iteratively get linked blocks
		access_point = FAT[access_point];
	}

	return access_point;
}

int fs_read(int fd, void *buf, size_t count)
{
	uint32_t total_count = 0;	// Max read is 8192 * 4096 bytes
	uint16_t read_index;		// Block to read from

	UNUSED(count);
	UNUSED(total_count);

	/* Error Checking */
	// Check if FS is mounted
	if (superblock.sig != SIGNATURE)
		fs_error("Filesystem not mounted");

	// Check if file descriptor is closed or out of bounds
	if (fd_list[fd].entry == NULL || fd >= FS_OPEN_MAX_COUNT)
		fs_error("Invalid file descriptor");

	if (buf == NULL)
		fs_error("buf is NULL");

	/* Begin Read */

	/* Perform read of certain section
	*
	*  1. Use a for loop to copy the bytes from bounce to buf
	*	- Use the offset member from struct file_descriptor to account for offset
	*  2. End the loop if the end of the data block is reached
	*  3. If end of data block has been reached, refer to FAT block for next node
	*	- To do this, write a file using fs_ref.x and try to examine its FAT structure using bash od
	*  4. Continue this process until count or EOF has been reached (while loop + break?)
	*/

	// Account for offset extending into another datablock
	read_index = fetch_offset_block_index(fd);

	// Get the first block of the file and read it into bounce buffer
	if (block_read(read_index, &bounce) < 0)
		fs_error("block_read");

	//while () {

		// Continue from the remaining offset 
		for (int i = fd_list[fd].offset % BLOCK_SIZE; i < BLOCK_SIZE; ++i) {


		}
	//}

	return 0;
}

