;Copyright 2016 Pedro Falcato
;
;Licensed under the Apache License, Version 2.0 (the "License");
;you may not use this file except in compliance with the License.
;You may obtain a copy of the License at
;
;http ://www.apache.org/licenses/LICENSE-2.0
;
;Unless required by applicable law or agreed to in writing, software
;distributed under the License is distributed on an "AS IS" BASIS,
;WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
;See the License for the specific language governing permissions and
;limitations under the License.
bits 32
section .multiboot
align 4
hdr_start:
	dd 0xe85250d6
	dd 0
	dd hdr_end - hdr_start
	dd 0x100000000 - (0xe85250d6 + 0 + (hdr_end - hdr_start))
	align 8 ;All tags need to be 8 byte aligned
	;Framebuffer tag
	dw 5
	dw 0
	dd 20
	dd 1024
	dd 768
	dd 32
	align 8
	; Module alignment tag
	dw 6
	dw 0
	dd 12
	align 8
	;Finish tag
	dw 0
	dw 0
	dd 8
hdr_end:
[SECTION .boot]
[GLOBAL EntryPoint]
EntryPoint:
	cli
	push eax
	push ebx
	mov eax, gdtr1
	lgdt [eax]

	push 0x08
	push .gdtrdy
	retf

.gdtrdy:
	mov eax, 0x10
	mov ds, ax
	mov ss, ax

	call setup_paging_and_longm

	mov eax, gdtr2
	lgdt [gdtr2]

	push 0x08
	push .gdt2rdy
	retf

[BITS 64]
.gdt2rdy:
	mov eax, 0x10
	mov ds, ax
	mov es, ax
	mov ss, ax
	
	; If you later decide to unmap the lower zone, you will have an invalid GDT if you're still using gdtr2
	mov rax, gdtr3
	lgdt [rax]
	pop rbx
	xor rsi,rsi
	mov rsi, rbx
	shr rsi, 32
	mov r8,rbx
	xor rdi,rdi
	mov edi,r8d
	mov rax, _start
	jmp rax
section .bootstrap_stack, nobits
align 4
stack_bottom:
resb 16384
stack_top:
section .text
extern KernelEarly
extern KernelMain
extern _init
_start:
	mov rsp, stack_top
	call InitSSE
	call KernelEarly
	call _init
	call KernelMain
	cli
.Lhang:
	hlt
	jmp .Lhang

section .data
	ErrorMSG db 'Spartix needs SSE to be supported!', 0
section .text
global InitSSE
InitSSE:
	mov rax, 0x1
	cpuid
	test edx, 1<<25
	jz .noSSE
	mov rax, cr0
	and ax, 0xFFFB
	or ax, 0x2
	mov cr0, rax
	mov rax, cr4
	or ax, 3 << 9
	mov cr4, rax
	ret
.noSSE:
	hlt
section .boot
[BITS 32]
[EXTERN pml4]
[EXTERN pdpt]
[EXTERN pd]
setup_paging_and_longm:
	mov eax, pdpt
	or eax, 3
	mov [pml4], eax
	mov [pml4 + 0xFF8], eax
	mov eax, pd
	or eax, 3
	or eax, 1 << 8
	mov [pdpt], eax
	mov [pdpt + 0xFF0], eax

	mov dword [pd], 0x000083
	or dword [pd], 1 << 8
	mov dword [pd + 8], 0x200083
	or dword [pd + 8], 1 << 8
	mov dword [pd + 16], 0x400083
	or dword [pd + 16], 1 << 8
	mov dword [pd + 24], 0x600083
	or dword [pd + 24], 1 << 8
	; Load CR3 with PML4
	mov eax, pml4
	mov cr3, eax

	; Enable PAE
	mov eax, cr4
	or eax, 1 << 5
	mov cr4, eax

	; Enable Long Mode in the MSR
	mov ecx, 0xC0000080
	rdmsr
	or eax, 1 << 8
	or eax, 1 << 11
	wrmsr

	; Enable Paging
	mov eax, cr0
	or eax, 1 << 31
	mov cr0, eax

	ret

tmp_gdt:
	DQ	0x0000000000000000
	DQ	0x00CF9A000000FFFF
	DQ	0x00CF92000000FFFF
	DQ	0x0000000000000000
	DQ	0x00A09A0000000000
	DQ	0x00A0920000000000
	DQ	0x00A0FA0000000000
	DQ	0x00A0F20000000000

gdtr1:
	DW	40
	DD	tmp_gdt

gdtr2:
	DW	40
	DD	tmp_gdt + 24
	DD	0

gdtr3:
	DW	40
	DQ	tmp_gdt + 24 + 0xFFFFFFFF80000000