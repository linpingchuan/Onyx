/*
* Copyright (c) 2016, 2017 Pedro Falcato
* This file is part of Onyx, and is released under the terms of the MIT License
* check LICENSE at the root directory for more information
*/
#ifndef _PAGECACHE_H
#define _PAGECACHE_H

#include <onyx/mutex.h>
#include <onyx/paging.h>
#include <onyx/list.h>
#include <onyx/vfs.h>

struct page_cache
{
	void *page;
	struct inode *node; /* IF it's actually a file */
	size_t size; /* Max value: PAGE_CACHE_SIZE */
	off_t offset;
	volatile _Atomic long dirty;
	mutex_t lock;
};

#define PAGE_CACHE_SIZE 65536 /* Each component of the cache has 64KiB */

struct page_cache *add_to_cache(void *data, size_t size, off_t off, struct inode *node);
void pagecache_init(void);
void wakeup_sync_thread(void);

#endif
