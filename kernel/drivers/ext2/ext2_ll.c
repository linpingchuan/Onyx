/*
* Copyright (c) 2017 Pedro Falcato
* This file is part of Onyx, and is released under the terms of the MIT License
* check LICENSE at the root directory for more information
*/
#include <mbr.h>
#include <partitions.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <dirent.h>

#include <sys/types.h>

#include <kernel/vmm.h>
#include <kernel/vfs.h>
#include <kernel/compiler.h>
#include <kernel/dev.h>
#include <kernel/log.h>
#include <kernel/fscache.h>

#include <drivers/rtc.h>
#include <drivers/ext2.h>

void *ext2_read_block(uint32_t block_index, uint16_t blocks, ext2_fs_t *fs)
{
	size_t size = blocks * fs->block_size; /* size = nblocks * block size */
	void *buff = NULL;
	/*if((buff = fscache_try_to_find_block(fs->first_sector + ((block_index * fs->block_size) % 512), fs->blkdevice, size)))
		return buff;*/
	buff = malloc(size); /* Allocate a buffer */
	if(!buff)
		return NULL;
	memset(buff, 0, size);
	size_t read = blkdev_read(fs->first_sector * 512 + (block_index * fs->block_size), size, buff, fs->blkdevice);
	//fscache_cache_sectors(buff, fs->blkdevice, fs->first_sector + ((block_index * fs->block_size) % 512), size);
	if(read == (size_t) -1)
	{
		free(buff);
		return NULL;
	}
	return buff;
}
void ext2_read_block_raw(uint32_t block_index, uint16_t blocks, ext2_fs_t *fs, void *buffer)
{
	size_t size = blocks * fs->block_size; /* size = nblocks * block size */

	blkdev_read(fs->first_sector * 512 + (block_index * fs->block_size), size, buffer, fs->blkdevice);
}
void ext2_write_block(uint32_t block_index, uint16_t blocks, ext2_fs_t *fs, void *buffer)
{
	size_t size = blocks * fs->block_size; /* size = nblocks * block size */
	blkdev_write(fs->first_sector * 512 + (block_index * fs->block_size), size, buffer, fs->blkdevice);
}
void __ext2_update_atime(inode_t *ino, uint32_t block, ext2_fs_t *fs, inode_t *inode_table)
{
	/* Skip atime updating if the inode doesn't want to */
	if(ino->flags & EXT2_INO_FLAG_ATIME_NO_UPDT)
		return;
	/* Update atime */
	ino->atime = (uint32_t) get_posix_time();
	ext2_write_block(block, 1, fs, inode_table);
}
inline void __ext2_update_ctime(inode_t *ino)
{
	ino->ctime = (uint32_t) get_posix_time();
}
inode_t *ext2_get_inode_from_number(ext2_fs_t *fs, uint32_t inode)
{
	uint32_t block_size = fs->block_size;
	uint32_t bg = (inode - 1) / fs->inodes_per_block_group;
	uint32_t index = (inode - 1) % fs->inodes_per_block_group;
	uint32_t block = (index * fs->inode_size) / block_size;
	uint32_t blockind = (index * fs->inode_size) % block_size;
	block_group_desc_t *bgd = &fs->bgdt[bg];
	inode_t *inode_table = NULL;
	inode_t *inode_block = (inode_t*)((char *) (inode_table = ext2_read_block(bgd->inode_table_addr + block, 1, fs)) + blockind);
	
	if(!inode_block)
		return NULL;
	
	/* Update the atime field */
	__ext2_update_atime(inode_block, bgd->inode_table_addr + block, fs, inode_table);
	return inode_block;
}
void ext2_update_inode(inode_t *ino, ext2_fs_t *fs, uint32_t inode)
{
	uint32_t block_size = fs->block_size;
	uint32_t bg = (inode - 1) / fs->inodes_per_block_group;
	uint32_t index = (inode - 1) % fs->inodes_per_block_group;
	uint32_t block = (index * fs->inode_size) / block_size;
	uint32_t blockind = (index * fs->inode_size) % block_size;
	block_group_desc_t *bgd = &fs->bgdt[bg];
	inode_t *inode_table = NULL;
	inode_t *inode_block = (inode_t*)((char *) (inode_table = ext2_read_block(bgd->inode_table_addr + block, 1, fs)) + blockind);
	if(!inode_block)
		return;
	__ext2_update_ctime(ino);
	memcpy(inode_block, ino, fs->inode_size);
	ext2_write_block(bgd->inode_table_addr + block, 1, fs, inode_table);
	free(inode_table);
}
