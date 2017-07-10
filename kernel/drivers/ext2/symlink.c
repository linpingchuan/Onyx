/*
* Copyright (c) 2017 Pedro Falcato
* This file is part of Onyx, and is released under the terms of the MIT License
* check LICENSE at the root directory for more information
*/
#include <stdint.h>
#include <stdbool.h>

#include <drivers/ext2.h>
/* According to Linux and e2fs, this is how you detect fast symlinks */
bool ext2_is_fast_symlink(inode_t *inode, ext2_fs_t *fs)
{
	int ea_blocks = inode->file_acl ? (fs->block_size >> 9) : 0;
	return (inode->disk_sects - ea_blocks == 0 && EXT2_CALCULATE_SIZE64(inode) <= 60);
}
char *ext2_do_fast_symlink(inode_t *inode)
{
	char *buf = malloc(60);
	if(!buf)
		return NULL;
	memcpy(buf, &inode->dbp, 60);
	return buf;
}
char *ext2_do_slow_symlink(inode_t *inode, ext2_fs_t *fs)
{
	char *buf = malloc(EXT2_CALCULATE_SIZE64(inode));
	if(!buf)
		return NULL;
	ext2_read_inode(inode, fs, EXT2_CALCULATE_SIZE64(inode), 0, buf);
	return buf;
}
char *ext2_read_symlink(inode_t *inode, ext2_fs_t *fs)
{
	if(ext2_is_fast_symlink(inode, fs))
	{
		return ext2_do_fast_symlink(inode);
	}
	else
	{
		return ext2_do_slow_symlink(inode, fs);
	}
}
