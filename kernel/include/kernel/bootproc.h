/*
* Copyright (c) 2017 Pedro Falcato
* This file is part of Onyx, and is released under the terms of the MIT License
* check LICENSE at the root directory for more information
*/
#ifndef _BOOTPROC_KERNEL_H
#define _BOOTPROC_KERNEL_H
#include <stdint.h>
#include <stddef.h>
#include <efi/efi.h>

struct framebuffer
{
	unsigned long framebuffer;
	unsigned long framebuffer_size;
	unsigned long height;
	unsigned long width;
	unsigned long bpp;
	unsigned long pitch;
};
struct module
{
	char name[256];
	uint64_t start;
	uint64_t size;
	struct modules *next;
};
struct efi_mmap_info
{
	EFI_MEMORY_DESCRIPTOR *descriptors;
	UINTN nr_descriptors;
	UINTN size_descriptors;
	UINTN desc_ver;
};
struct boot_info
{
	struct framebuffer fb;
	struct module *modules;
	EFI_SYSTEM_TABLE *SystemTable;
	struct efi_mmap_info mmap;
	wchar_t *command_line;
};
#endif
