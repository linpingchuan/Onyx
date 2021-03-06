/*
* Copyright (c) 2017 Pedro Falcato
* This file is part of Onyx, and is released under the terms of the MIT License
* check LICENSE at the root directory for more information
*/

#ifndef _ONYX_SUPERBLOCK_H
#define _ONYX_SUPERBLOCK_H

#include <sys/types.h>

#include <onyx/spinlock.h>

struct inode;

struct superblock
{
	struct inode *s_inodes;
	unsigned long s_ref;
	void *s_helper;
	spinlock_t s_ilock;
};

struct inode *superblock_find_inode(struct superblock *sb, ino_t inode);
void superblock_add_inode(struct superblock *sb, struct inode *inode);
void superblock_remove_inode(struct superblock *sb, struct inode *inode);

#endif
