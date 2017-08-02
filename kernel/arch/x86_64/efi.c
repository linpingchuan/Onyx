/*
* Copyright (c) 2017 Pedro Falcato
* This file is part of Onyx, and is released under the terms of the MIT License
* check LICENSE at the root directory for more information
*/
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <efi/efi.h>

#include <kernel/bootproc.h>
#include <kernel/paging.h>
#include <kernel/idt.h>
#include <kernel/vmm.h>
#include <kernel/cpu.h>
#include <kernel/tty.h>
#include <kernel/page.h>
#include <kernel/random.h>

#include <drivers/softwarefb.h>
#include <drivers/rtc.h>

extern PML1 pt_mappings;
extern uint64_t kernel_end;

void *paging_map_temp(void *phys, size_t num)
{
	for(size_t i = 0; i < num; i++)
	{
		pt_mappings.entries[i] = ((uintptr_t) phys & 0xFFFFFFFFFFFFF000) | 3;
		phys = (void*)((uintptr_t) phys + PAGE_SIZE);
	}
	__asm__ __volatile__("invlpg 0xFFFFFFFF80600000");
	return (void*) 0xFFFFFFFF80600000;
}
bool uefi_unusable_region(EFI_MEMORY_DESCRIPTOR *desc)
{
	/* We can't use EfiLoaderData regions as they're important 
	 * Maybe we can use another memory desc type in the bootloader 
	 * for more efficient memory usage
	*/
	if(desc->Type == EfiLoaderData)
		return true;
	/* Get rid of the boot services' code and data */
	if(desc->Type == EfiBootServicesCode)
		return true;
	if(desc->Type == EfiBootServicesData)
		return true;
	/* We can't use EfiUnusableMemory memory descs because I don't fucking know, 
	 * UEFI marks some random areas as unusable memory
	 * Anyway, don't use this memory unless we want to brick firmware or some shit
	*/
	if(desc->Type == EfiUnusableMemory)
		return true;
	if(desc->Type == EfiReservedMemoryType)
		return true;
	/* Memory mapped I/O is also unusable */
	if(desc->Type == EfiMemoryMappedIO)
		return true;
	if(desc->Type == EfiMemoryMappedIOPortSpace)
		return true;
	/* We also can't use ACPI memory */
	if(desc->Type == EfiACPIReclaimMemory)
		return true;
	if(desc->Type == EfiACPIMemoryNVS)
		return true;
	/* Lets not overwrite runtime services memory, shall we?
	 * We'll need this memory to call the runtime services if the user wants
	 * to i.e set an NVRAM variable
	*/
	if(desc->Type == EfiRuntimeServicesCode)
		return true;
	if(desc->Type == EfiRuntimeServicesData)
		return true;
	/* TODO: EfiMaxMemoryType and EfiPalCode */
	return false;
}
void uefi_main(struct boot_info *info)
{
	printf("Booting through UEFI\n");
	/* Initialize the IDT and the physical memory map */
	idt_init();
	vmm_init();

	/* Get the boot info */
	info = (struct boot_info*)((uintptr_t) info + PHYS_BASE);
	printf("Memory map info: %u entries - address %p\n", info->mmap.nr_descriptors, 
								info->mmap.descriptors);
	/* Loop through the memory descriptors and parse them
	 * Note that sizeof(EFI_MEMORY_DESCRIPTOR) doesn't necessarily equal info->mmap.size_descriptors
	 * At least on OVMF it's different, which means almost every UEFI implementation does this
	 * since they're almost all using Tianocore
	*/
	/* First, we're counting the amount of available memory in the system */
	size_t memory = 0;
	EFI_MEMORY_DESCRIPTOR *descriptors = (void*) ((uintptr_t) info->mmap.descriptors + PHYS_BASE);
	for(UINTN i = 0; i < info->mmap.nr_descriptors; i++, descriptors = (void*) (uintptr_t) descriptors + info->mmap.size_descriptors)
	{
		if((descriptors->PhysicalStart + descriptors->NumberOfPages * PAGE_SIZE) > memory)
			memory = descriptors->PhysicalStart + descriptors->NumberOfPages * PAGE_SIZE;
	}
	/* Initialize bootmem */
	bootmem_init(memory, (uintptr_t) &kernel_end);
	descriptors = (void*) ((uintptr_t) info->mmap.descriptors + PHYS_BASE);
	struct module *module = (struct module *)((uintptr_t) info->modules + PHYS_BASE);
	for(UINTN i = 0; i < info->mmap.nr_descriptors; i++, descriptors = (void*) (uintptr_t) descriptors + info->mmap.size_descriptors)
	{
		if(uefi_unusable_region(descriptors))
			continue;
		bootmem_push(descriptors->PhysicalStart, descriptors->NumberOfPages * PAGE_SIZE, module);
	}
	/* Identify the CPU it's running on (bootstrap CPU) */
	cpu_identify();

	paging_map_all_phys();
	printf("Framebuffer: %p\n", info->fb.framebuffer);
	/* Map the FB */
	for (uintptr_t virt = KERNEL_FB, phys = info->fb.framebuffer; virt < KERNEL_FB + info->fb.framebuffer_size; virt += 4096, phys += 4096)
	{
		paging_map_phys_to_virt(virt, phys, VMM_WRITE | VMM_NOEXEC);
	}
	/* Initialize the Software framebuffer */
	softfb_init(KERNEL_FB, info->fb.bpp, info->fb.width, info->fb.height, info->fb.pitch);

	/* Initialize the first terminal */
	tty_init();
	page_init();
	printk("HYPE BOIS\n");
	/* Map the first bucket's memory address */
	void *mem = (void*)0xFFFFFFF890000000;
	vmm_map_range(mem, 1024, VMM_GLOBAL | VMM_WRITE | VMM_NOEXEC);

	/* We need to get some early boot rtc data and initialize the entropy, as it's vital to initialize
	 * some entropy sources for the memory map */
	early_boot_rtc();
	initialize_entropy();

	vmm_start_address_bookkeeping(KERNEL_FB, 0xFFFFFFF890000000);
	while(1);
}
