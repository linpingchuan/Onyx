/*
* Copyright (c) 2017 Pedro Falcato
* This file is part of Onyx, and is released under the terms of the MIT License
* check LICENSE at the root directory for more information
*/

#ifndef _ONYX_X86_MSR_H
#define _ONYX_X86_MSR_H

#define FS_BASE_MSR 		0xC0000100
#define GS_BASE_MSR 		0xC0000101
#define KERNEL_GS_BASE 		0xC0000102
#define IA32_MSR_STAR 		0xC0000081
#define IA32_MSR_LSTAR 		0xC0000082
#define IA32_MSR_CSTAR 		0xC0000083
#define IA32_MSR_SFMASK 	0xC0000084
#define IA32_MSR_MC0_CTL 	0x00000400
#define IA32_VMX_BASIC          0x00000480
#define IA32_VMX_CR0_FIXED0     0x00000486
#define IA32_VMX_CR0_FIXED1     0x00000487
#define IA32_VMX_CR4_FIXED0     0x00000488
#define IA32_VMX_CR4_FIXED1     0x00000489

#endif
