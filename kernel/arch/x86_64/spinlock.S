section .text
global spinlock_lock
spinlock_lock:
	push rbp
	mov rbp, rsp
.retry:
	pause
	mov rax, 1
	xchg [rdi], rax
	cmp rax, 1
	je .retry
	pop rbp
	ret
global spinlock_unlock
spinlock_unlock:
	push rbp
	mov rbp, rsp
	xor rax, rax
	xchg [rdi], rax
	pop rbp
	ret
