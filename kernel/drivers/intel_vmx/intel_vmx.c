/*
* Copyright (c) 2017 Pedro Falcato
* This file is part of Onyx, and is released under the terms of the MIT License
* check LICENSE at the root directory for more information
*/

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <errno.h>

#include <onyx/cpu.h>
#include <onyx/vmm.h>
#include <onyx/driver.h>
#include <onyx/page.h>
#include <onyx/log.h>

#include "vmx.h"

/* Consult intel manuals appendix B for vmx encodings */
void vmxon(void *addr)
{
	__asm__ __volatile__("vmxon %0" :: "m"(addr));
}

void vmclear(void *addr)
{
	__asm__ __volatile__("vmclear %0" :: "m"(addr));	
}

void vmptrld(void *addr)
{
	__asm__ __volatile__("vmptrld %0" :: "m"(addr));
}

static spinlock_t vmx_lock;

struct vmx_context *__vmx_create_context(void)
{
	void *vmxon_region = __alloc_page(0);
	if(!vmxon_region)
		return NULL;
	
	struct vmx_context *vmxctx = zalloc(sizeof *vmxctx);
	if(!vmxctx)
	{
		__free_page(vmxon_region);
		return NULL;
	}

	vmxctx->vmxon_region = vmxon_region;
	vmxon(vmxon_region);

	return vmxctx;
}

struct vmx_context *vmx_create_context(void)
{
	if(try_and_acquire_spinlock(&vmx_lock) == 1)
		return errno = EBUSY, NULL;
	struct vmx_context *ctx = __vmx_create_context();

	if(!ctx)
	{
		release_spinlock(&vmx_lock);
		return NULL;
	}

	return ctx;
}
void vmx_init(void)
{
	if(x86_has_cap(X86_FEATURE_VMX))
	{
		INFO("vmx", "Host CPU supports VMX\n");
		uint64_t cr4 = cpu_get_cr4();
		cr4 |= X86_CR4_VMXE;
		
		/* Enable VMX by loading the new cr4 */
		__asm__ __volatile__("mov %0, %%cr4" :: "r"(cr4));
	}
}

DRIVER_INIT(vmx_init);
