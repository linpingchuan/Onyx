/*
* Copyright (c) 2016, 2017 Pedro Falcato
* This file is part of Onyx, and is released under the terms of the MIT License
* check LICENSE at the root directory for more information
*/
#ifndef _KERNEL_REGISTERS_H
#define _KERNEL_REGISTERS_H

#include <stdint.h>
#ifdef __x86_64__

typedef struct registers
{
	uint64_t ds;
	uint64_t r15, r14, r13, r12, r11, r10, r9, r8, rbp, rsi, rdi, rdx, rcx, rbx, rax;
	uint64_t rip;
	uint64_t cs;
	uint64_t rflags;
	uint64_t rsp;
	uint64_t ss;
} registers_t;

typedef struct
{
	uint64_t ds;
	uint64_t r15, r14, r13, r12, r11, r10, r9, r8, rbp, rsi, rdi, rdx, rcx, rbx, rax;
	uint64_t int_no;
	uint64_t err_code;
	uint64_t rip;
	uint64_t cs;
	uint64_t rflags;
	uint64_t rsp;
	uint64_t ss;
} intctx_t;

typedef struct
{
	uint64_t ds;
	uint64_t r15, r14, r13, r12, r11, r10, r9, r8, rbp, rsi, rdi, rdx, rcx, rbx;
	uint64_t rip;
	uint64_t cs;
	uint64_t rflags;
	uint64_t rsp;
	uint64_t ss;
} syscall_ctx_t;

static inline void wrmsr(uint32_t msr, uint32_t lo, uint32_t hi)
{
	__asm__ __volatile__("wrmsr"::"a"(lo), "d"(hi), "c"(msr));
}

static inline void rdmsr(uint32_t msr, uint32_t *lo, uint32_t *hi)
{
	__asm__ __volatile__("rdmsr" : "=a"(*lo), "=d"(*hi) : "c"(msr));
}

#include <onyx/x86/msr.h>

#define X86_CR4_VME		(1 << 0)
#define X86_CR4_PVI		(1 << 1)
#define X86_CR4_TSD		(1 << 2)
#define X86_CR4_DE		(1 << 3)
#define X86_CR4_PSE		(1 << 4)
#define X86_CR4_PAE		(1 << 5)
#define X86_CR4_MCE		(1 << 6)
#define X86_CR4_PGE		(1 << 7)
#define X86_CR4_PCE		(1 << 8)
#define X86_CR4_OSFXSR		(1 << 9)
#define X86_CR4_OSXMMEXCPT	(1 << 10)
#define X86_CR4_VMXE		(1 << 13)
#define X86_CR4_SMXE		(1 << 14)
#define X86_CR4_PCID		(1 << 17)
#define X86_CR4_OSXSAVE		(1 << 18)
#define X86_CR4_SMEP		(1 << 20)
#define X86_CR4_SMAP		(1 << 21)

#endif
#endif
