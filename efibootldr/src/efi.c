/*
* Copyright (c) 2017 Pedro Falcato
* This file is part of Onyx, and is released under the terms of the MIT License
* check LICENSE at the root directory for more information
*/
#include <efi.h>
#include <efilib.h>
#include <efiprot.h>
#include <stddef.h>
#include <stdint.h>

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
struct boot_info *boot_info = NULL;
void append_module(struct module *module)
{
	if(!boot_info->modules)
	{
		boot_info->modules = module;
	}
	else
	{
		struct module *m = boot_info->modules;
		while(m->next) m = m->next;
		m->next = module;
	}
}
int initialize_graphics(EFI_SYSTEM_TABLE *SystemTable)
{
	EFI_STATUS st = 0;
	EFI_GRAPHICS_OUTPUT_PROTOCOL *gop = NULL;
	EFI_GUID guid = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
	st = SystemTable->BootServices->LocateProtocol(&guid, NULL, &gop);
	if(st != EFI_SUCCESS)
	{
		Print(L"Failed to get the GOP protocol - error code 0x%x\n", st);
		return -1;
	}
	EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE *mode = gop->Mode;
	UINT32 to_be_set = 0;
	/* We're picking the biggest graphics framebuffer out of them all (it's usually a good idea) */
	UINT32 total_space = 0;
	for(UINT32 i = 0; i < mode->MaxMode; i++)
	{
		UINTN size;
		UINTN bpp;
		EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *info;
		st = gop->QueryMode(gop, i, &size, &info);
		if(st != EFI_SUCCESS)
		{
			Print(L"Failed to get video mode %u - error code 0x%x\n", i, st);
			return -1;
		}
		
		/* Ignore non-linear video modes */
		if(info->PixelFormat == PixelBltOnly)
			continue;
		if(info->PixelFormat == PixelBlueGreenRedReserved8BitPerColor || info->PixelFormat == PixelRedGreenBlueReserved8BitPerColor)
			bpp = 32;
		else if(info->PixelFormat == PixelBitMask)
		{
			/* How should this work? I think this works kinda fine? I'm not sure we even want 
			 * to support non-32 bit pixel formats
			*/
			bpp = 32;
		}
		UINT32 fb_size = info->HorizontalResolution * info->VerticalResolution * (bpp / 8);
		if(fb_size > total_space)
		{
			to_be_set = i;
			total_space = fb_size;
		}
	}
	/* Set the video mode */
	gop->SetMode(gop, to_be_set);
	mode = gop->Mode;
	boot_info->fb.framebuffer = mode->FrameBufferBase;
	boot_info->fb.framebuffer_size = mode->FrameBufferSize;
	boot_info->fb.height = mode->Info->HorizontalResolution;
	boot_info->fb.width = mode->Info->VerticalResolution;
	boot_info->fb.pitch = mode->Info->PixelsPerScanLine;
	/* TODO: This shouldn't be a thing? */
	boot_info->fb.bpp = 32;
	return 0;
}
void *elf_load(void *buffer, size_t size);
void *load_kernel(EFI_FILE_PROTOCOL *root, EFI_SYSTEM_TABLE *SystemTable)
{
	EFI_STATUS st;
	EFI_FILE_PROTOCOL *kernel;
	EFI_GUID file_info = EFI_FILE_INFO_ID;
	void *buffer;
	UINTN size;
	EFI_FILE_INFO *info;
	if((st = root->Open(root, &kernel, L"vmonyx", EFI_FILE_MODE_READ, 0)) != EFI_SUCCESS)
	{
		Print(L"Error: Open(): Could not open vmonyx - error code 0x%x\n", st);
		return NULL;
	}
	if((st = SystemTable->BootServices->AllocatePool(EfiLoaderData, sizeof(EFI_FILE_INFO), &info)) != EFI_SUCCESS)
	{
		Print(L"Error: AllocatePool: Could not get memory - error code 0x%x\n", st);
		return NULL;
	}
	size = sizeof(EFI_FILE_INFO);
retry:
	if((st = kernel->GetInfo(kernel, &file_info, &size, (void*) info)) != EFI_SUCCESS)
	{
		if(st == EFI_BUFFER_TOO_SMALL)
		{
			SystemTable->BootServices->FreePool(info);
			if((st = SystemTable->BootServices->AllocatePool(EfiLoaderData, size, &info)) != EFI_SUCCESS)
			{
				Print(L"Error: AllocatePool: Could not get memory - error code 0x%x\n", st);
				return NULL;
			}
			goto retry;
		}
		Print(L"Error: GetInfo: Could not get file info - error code 0x%x\n", st);
		return NULL;
	}
	size = info->FileSize;
	if((st = SystemTable->BootServices->AllocatePool(EfiLoaderData, size, &buffer)) != EFI_SUCCESS)
	{
		SystemTable->BootServices->FreePool(info);
		Print(L"Error: AllocatePool: Could not get memory - error code 0x%x\n", st);
		return NULL;
	}
	if((st = kernel->Read(kernel, &size, buffer)) != EFI_SUCCESS)
	{
		Print(L"Error: Read: Could not read the file - error code 0x%x\n", st);
		SystemTable->BootServices->FreePool(info);
		SystemTable->BootServices->FreePool(buffer);
		return NULL;
	}
	void *entry_point = elf_load(buffer, size);
	return entry_point;
}
int efi_exit_firmware(void *entry, EFI_SYSTEM_TABLE *SystemTable, EFI_HANDLE ImageHandle)
{
	EFI_STATUS st;
	UINTN nr_entries;
	UINTN key;
	UINTN desc_size;
	UINT32 desc_ver;
	EFI_MEMORY_DESCRIPTOR *map = LibMemoryMap(&nr_entries, &key, &desc_size, &desc_ver);
	if(!map)
	{
		Print(L"Error: LibMemoryMap: Could not get the memory map\n");
		return -1;
	}
	boot_info->mmap.desc_ver = desc_ver;
	boot_info->mmap.descriptors = map;
	boot_info->mmap.nr_descriptors = nr_entries;
	boot_info->mmap.size_descriptors = desc_size;
	if((st = SystemTable->BootServices->ExitBootServices(ImageHandle, key)) != EFI_SUCCESS)
	{
		Print(L"Error: ExitBootServices: Could not exit the boot services - error code 0x%x\n", st);
		return -1;
	}
	void (*entry_point)(struct boot_info *boot) = (void (*)(struct boot_info*)) entry;
	entry_point(boot_info);
	return 0;
}
int load_initrd(EFI_FILE_PROTOCOL *root, EFI_SYSTEM_TABLE *SystemTable);
EFI_STATUS
efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable)
{
	EFI_STATUS st;
	EFI_GUID guid = EFI_LOADED_IMAGE_PROTOCOL_GUID;
	EFI_GUID device_protocol = EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID;
	EFI_LOADED_IMAGE_PROTOCOL *image;
	EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *fs;
	EFI_FILE_PROTOCOL *root;
	InitializeLib(ImageHandle, SystemTable);
	Print(L"Onyx efibootldr - booting...\n");

	if(SystemTable->BootServices->AllocatePool(EfiLoaderData, sizeof(struct boot_info), &boot_info) != EFI_SUCCESS)
		return EFI_LOAD_ERROR;
	boot_info->modules = NULL;
	if(initialize_graphics(SystemTable) < 0)
		return EFI_LOAD_ERROR;
	SystemTable->ConOut->ClearScreen(SystemTable->ConOut);
	/* Get a protocol to the loaded image */
	if((st = SystemTable->BootServices->HandleProtocol(ImageHandle, &guid, &image)) != EFI_SUCCESS)
	{
		Print(L"Error: HandleProtocol(): Failed to get EFI_LOADED_IMAGE_PROTOCOL_GUID - error code 0x%x\n", st);
		return EFI_LOAD_ERROR;
	}
	boot_info->command_line = image->LoadOptions;
	/* Get a protocol to the loaded image */
	if((st = SystemTable->BootServices->HandleProtocol(image->DeviceHandle, &device_protocol, &fs)) != EFI_SUCCESS)
	{
		Print(L"Error: HandleProtocol(): Failed to get EFI_DEVICE_IO_PROTOCOL_GUID - error code 0x%x\n", st);
		return EFI_LOAD_ERROR;
	}
	if((st = fs->OpenVolume(fs, &root)) != EFI_SUCCESS) 
	{
		Print(L"Error: OpenVolume: Failed to open the root file - error code 0x%x\n", st);
		return EFI_LOAD_ERROR;
	}
	void *entry_point = NULL;
	if(load_initrd(root, SystemTable) < 0)
		return EFI_LOAD_ERROR;
	if((entry_point = load_kernel(root, SystemTable)) == NULL)
		return EFI_LOAD_ERROR;
	/* Exit the firmware and run the kernel */
	if(efi_exit_firmware(entry_point, SystemTable, ImageHandle) < 0)
		return EFI_LOAD_ERROR;
	for(;;) __asm__("hlt");
}
