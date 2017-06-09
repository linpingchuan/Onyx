/*
* Copyright (c) 2016, 2017 Pedro Falcato
* This file is part of Onyx, and is released under the terms of the MIT License
* check LICENSE at the root directory for more information
*/
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

#include <sys/resource.h>

#include <kernel/vdso.h>
#include <kernel/compiler.h>
#include <kernel/elf.h>
#include <kernel/process.h>
#include <kernel/task_switching.h>
#include <kernel/cpu.h>
#include <kernel/random.h>
#include <kernel/mutex.h>
#include <kernel/panic.h>
#include <kernel/binfmt.h>

#include <pthread_kernel.h>

extern PML4 *current_pml4;
process_t *first_process = NULL;
volatile process_t *current_process = NULL;
static pid_t current_pid = 1;
static spinlock_t process_creation_lock;
int copy_file_descriptors(process_t *process, ioctx_t *ctx)
{
	process->ctx.file_desc = malloc(ctx->file_desc_entries * sizeof(void*));
	process->ctx.file_desc_entries = ctx->file_desc_entries;
	if(!process->ctx.file_desc)
		return -1;
	for(int i = 0; i < process->ctx.file_desc_entries; i++)
	{
		process->ctx.file_desc[i] = ctx->file_desc[i];
		if(ctx->file_desc[i])
			ctx->file_desc[i]->refcount++;
	}
	return 0;
}
int allocate_file_descriptor_table(process_t *process)
{
	process->ctx.file_desc = malloc(UINT8_MAX * sizeof(void*));
	if(!process->ctx.file_desc)
	{
		return -1;
	}
	memset(process->ctx.file_desc, 0, UINT8_MAX * sizeof(void*));
	process->ctx.file_desc_entries = UINT8_MAX;
	return 0;
}
process_t *process_create(const char *cmd_line, ioctx_t *ctx, process_t *parent)
{
	process_t *proc = malloc(sizeof(process_t));
	if(!proc)
		return errno = ENOMEM, NULL;
	memset(proc, 0, sizeof(process_t));
	acquire_spinlock(&process_creation_lock);
	proc->pid = current_pid++;
	proc->cmd_line = (char*) cmd_line;
	if(ctx)
	{
		if(copy_file_descriptors(proc, ctx) < 0)
		{
			free(proc);
			release_spinlock(&process_creation_lock);
			return NULL;
		}
	}
	else
	{
		if(allocate_file_descriptor_table(proc) < 0)
		{
			free(proc);
			release_spinlock(&process_creation_lock);
			return NULL;
		}
	}
	if(parent)
		proc->parent = parent;
	if(!first_process)
		first_process = proc;
	else
	{
		process_t *it = (process_t*) get_current_process();
		while(it->next) it = it->next;
		it->next = proc;
	}
	release_spinlock(&process_creation_lock);
	return proc;
}
void process_create_thread(process_t *proc, thread_callback_t callback, uint32_t flags, int argc, char **argv, char **envp)
{
	thread_t *thread = NULL;
	if(!argv)
		thread = sched_create_thread(callback, flags, NULL);
	else
		thread = sched_create_main_thread(callback, flags, argc, argv, envp);
	int is_set = 0;
	for(int i = 0; i < THREADS_PER_PROCESS; i++)
	{
		if(proc->threads[i] == NULL)
		{
			proc->threads[i] = thread;
			thread->owner = proc;
			is_set = 1;
		}
	}
	if(!is_set)
		thread_destroy(thread);
}
extern int curr_id;
void process_fork_thread(process_t *dest, process_t *src, int thread_index)
{
	dest->threads[thread_index] = malloc(sizeof(thread_t));
	if(!dest->threads[thread_index])
		return errno = ENOMEM, (void) 0;
	memcpy(dest->threads[thread_index], src->threads[thread_index], sizeof(thread_t));
	dest->threads[thread_index]->next = NULL;
	thread_add(dest->threads[thread_index]);
	dest->threads[thread_index]->id = curr_id++;
	dest->threads[thread_index]->owner = dest;
}
process_t *get_process_from_pid(pid_t pid)
{
	process_t *p = first_process;
	for(;p;p = p->next)
	{
		if(p->pid == pid)
			return p;
	}
	return NULL;
}
char **process_copy_envarg(char **envarg, _Bool to_kernel, int *count)
{
	/* Copy the envp/argv to another buffer */
	/* Each buffer takes up argc * sizeof pointer + string_size + one extra pointer(to NULL terminate) */
	size_t nr_args = 0;
	size_t string_size = 0;
	char **b = envarg;
	while(*b)
	{
		string_size += strlen(*b) + 1;
		nr_args++;
		b++;
	}
	size_t buffer_size = (nr_args + 1) * sizeof(void*) + string_size;
	char *new;
	if(to_kernel)
	{
		new = malloc(buffer_size);
		if(!new)
			return NULL;
	}
	else
	{
		new = vmm_allocate_virt_address(0, vmm_align_size_to_pages(buffer_size), VM_TYPE_SHARED, VM_WRITE | VM_NOEXEC | VM_USER, 0);
		if(!new)
			return NULL;
		if(!vmm_map_range(new, vmm_align_size_to_pages(buffer_size), VM_WRITE | VM_NOEXEC | VM_USER))
			return NULL;
	}
	memset(new, 0, buffer_size);

	char *strings = (char*) new + (nr_args + 1) * sizeof(void*);
	char *it = strings;
	/* Actually copy the buffer */
	for(size_t i = 0; i < nr_args; i++)
	{
		strcpy(it, envarg[i]);
		it += strlen(envarg[i]) + 1;
	}
	char **new_args = (char**) new;
	for(size_t i = 0; i < nr_args; i++)
	{
		new_args[i] = (char*) strings;
		strings += strlen(new_args[i]) + 1;
	}
	if(count)
		*count = nr_args;
	return new_args;
}
void process_setup_auxv(void *buffer, process_t *process)
{
	/* Setup the auxv at the stack bottom */
	Elf64_auxv_t *auxv = (Elf64_auxv_t *) buffer;
	unsigned char *scratch_space = (unsigned char *) (auxv + 37);
	for(int i = 0; i < 38; i++)
	{
		if(i != 0)
			auxv[i].a_type = i;
		if(i == 37)
			auxv[i].a_type = 0;
		switch(i)
		{
			case AT_PAGESZ:
				auxv[i].a_un.a_val = PAGE_SIZE;
				break;
			case AT_UID:
				auxv[i].a_un.a_val = process->uid;
				break;
			case AT_GID:
				auxv[i].a_un.a_val = process->gid;
				break;
			case AT_RANDOM:
				get_entropy((char*) scratch_space, 16);
				scratch_space += 16;
				break;
		}
	}
	/* TODO: Do this portably */
	registers_t *regs = (registers_t *) process->threads[0]->kernel_stack;
	regs->rcx = (uintptr_t) auxv;
}
void process_setup_pthread(thread_t *thread, process_t *process)
{
	/* TODO: Do this portably */
	/* TODO: Return error codes and clean up */
	uintptr_t *fs = vmm_allocate_virt_address(0, 1, VM_TYPE_REGULAR, VMM_WRITE | VMM_NOEXEC | VMM_USER, 0);
	vmm_map_range(fs, 1, VMM_WRITE | VMM_NOEXEC | VMM_USER);
	thread->fs = (void*) fs;
	__pthread_t *p = (__pthread_t*) fs;
	p->self = (__pthread_t*) fs;
	p->tid = get_current_process()->threads[0]->id;
	p->pid = get_current_process()->pid;
}
int sys_execve(char *path, char *argv[], char *envp[])
{
	if(!vmm_is_mapped(path))
		return errno =-EINVAL;
	if(!vmm_is_mapped(argv))
		return errno =-EINVAL;
	if(!vmm_is_mapped(envp))
		return errno =-EINVAL;

	/* Create a new address space */
	avl_node_t *tree;
	PML4 *cr3 = vmm_clone_as(&tree);
	/* Open the file */
	vfsnode_t *in = open_vfs(fs_root, path);
	if (!in)
		return -ENOENT;
	/* TODO: Check file permitions */

	/* Copy argv and envp to the kernel space */
	int argc;
	char **karg = process_copy_envarg(argv, true, &argc);
	/* TODO: Abort process construction */
	if(!karg)
		return -ENOMEM;
	char **kenv = process_copy_envarg(envp, true, NULL);
	if(!kenv)
	{
		free(karg);
		return -ENOMEM;
	}	
	/* Swap address spaces. Good thing we saved argv and envp before */
	process_t *current = get_current_process();
	current->cr3 = cr3;
	current->tree = tree;
	current->mmap_base = vmm_gen_mmap_base();
	current->brk = vmm_reserve_address(vmm_gen_brk_base(), vmm_align_size_to_pages(0x2000000), VM_TYPE_REGULAR, VM_WRITE | VM_NOEXEC);
	
	current->cmd_line = strdup(path);
	paging_load_cr3(current->cr3);
	vmm_set_tree(tree);
	
	/* Setup the binfmt args */
	uint8_t *file = malloc(100);
	if(!file)
	{
		free(karg);
		free(kenv);
		return -ENOMEM;
	}
	/* Read the file signature */
	read_vfs(0, 100, file, in);
	struct binfmt_args args;
	args.file_signature = file;
	args.filename = current->cmd_line;
	args.argv = karg;
	args.envp = kenv;
	args.file = in;

	/* Load the actual binary */
	void *entry = load_binary(&args);
	if(!entry)
	{
		free(karg);
		free(kenv);
		free(file);
		return -errno;
	}
	free(file);

	/* Copy argv and envp to user space memory */
	char **uargv = process_copy_envarg(karg, false, NULL);
	if(!uargv)
		return -errno;
	char **uenv = process_copy_envarg(kenv, false, NULL);
	if(!uenv)
		return -errno;
	/* Free karg and kenv, we don't need them anymore  */
	free(karg);
	free(kenv);

	/* We need to disable interrupts here, since we're destroying threads and creating new ones
	(who may not be ready to execute) */
	DISABLE_INTERRUPTS();
	memset(current->threads, 0, sizeof(thread_t*) * THREADS_PER_PROCESS);
	/* Create the main thread */
	process_create_thread(current, (thread_callback_t) entry, 0, argc, uargv, uenv);

	/* Setup auxv */
	process_setup_auxv(current->threads[0]->user_stack_bottom, current);
	/* Setup the pthread structure */
	process_setup_pthread(current->threads[0], current);

	ENABLE_INTERRUPTS();
	while(1);
}
pid_t sys_getppid()
{
	if(get_current_process()->parent)
		return get_current_process()->parent->pid;
	else
		return -1;
}
pid_t sys_wait4(pid_t pid, int *wstatus, int options, struct rusage *usage)
{
	process_t *i = (process_t*) get_current_process();
	process_t *curr_process = i;
	_Bool found_child = 0;
	while(i)
	{
		if(i->parent == curr_process)
		{
			if(i->pid == pid)
				found_child = 1;
		}
		if(i->parent == curr_process && i->has_exited == 1 && i->pid == pid)
		{
			if(wstatus)
				*wstatus = i->exit_code;

			/* TODO: Destroy the zombie process */
			i->parent = NULL;
			return i->pid;
		}
		i = i->next;
		if(!i)
		{
			i = get_current_process();
			if(found_child == 0)
				return -ECHILD;
		}
	}
	return -ECHILD;
}
pid_t sys_fork(syscall_ctx_t *ctx)
{
	process_t *proc = (process_t*) get_current_process();
	
	/* If we don't get the process, idk what the hell is going on, so just return ENOMEM */
	if(!proc)
		return -ENOMEM;
	/* Create a new process */
	process_t *child = process_create(proc->cmd_line, &proc->ctx, proc); /* Create a process with the current
			  			  * process's info */
	if(!child)
		return -ENOMEM;

	/* Fork the vmm data and the address space */
	avl_node_t *areas;
	PML4 *new_pt = vmm_fork_as(&areas); // Fork the address space
	if(!new_pt)
	{
		/* TODO: Destroy the process */
		vmm_destroy_tree(areas);
		return -ENOMEM;
	}
	if(!areas)
	{
		/* TODO: Cleanup the paging structures */
		return -ENOMEM;
	}
	child->tree = areas;
	child->cr3 = new_pt; // Set the new cr3

	/* We need to disable the interrupts for a moment, because thread_add adds it to the queue, 
	   and the thread isn't ready yet */

	DISABLE_INTERRUPTS();
	/* Fork and create the new thread */
	process_fork_thread(child, proc, 0);
	child->threads[0]->kernel_stack = vmalloc(4, VM_TYPE_STACK, VM_WRITE | VM_NOEXEC | VM_GLOBAL);
	if(!child->threads[0]->kernel_stack)
	{
		free(child->threads[0]);
		thread_destroy(child->threads[0]);
		process_destroy_aspace(child);
		free(child);
		ENABLE_INTERRUPTS();
		return -ENOMEM;
	}
	child->threads[0]->kernel_stack = (uintptr_t *) ((unsigned char *)child->threads[0]->kernel_stack + 0x4000);
	child->threads[0]->kernel_stack_top = child->threads[0]->kernel_stack;
	child->threads[0]->kernel_stack = sched_fork_stack(ctx, child->threads[0]->kernel_stack);
	child->threads[0]->fs = get_current_process()->threads[0]->fs;
	ENABLE_INTERRUPTS();
	// Return the pid to the caller
	return child->pid;
}
void sys_exit(int status)
{
	process_t *current = get_current_process();
	if(current->pid == 1)
	{
		printf("Panic: %s returned!\n", get_current_process()->cmd_line);
		ENABLE_INTERRUPTS();
		for(;;);
	}
	current->has_exited = 1;
	current->exit_code = status;

	/* TODO: Support multi-threaded processes */
	thread_t *current_thread = get_current_thread();
	
	/* Destroy everything that can be destroyed now */
	thread_destroy(current_thread);

	sched_yield();
}
uint64_t sys_getpid()
{
	return get_current_process()->pid;
}
int sys_personality(unsigned long val)
{
	// TODO: Use this syscall for something. This might be potentially very useful
	get_current_process()->personality = val;
	return 0;
}
int sys_setuid(uid_t uid)
{
	get_current_process()->uid = uid;
	return 0;
}
int sys_setgid(gid_t gid)
{
	get_current_process()->gid = gid;
	return 0;
}
uid_t sys_getuid(void)
{
	return get_current_process()->uid;
}
gid_t sys_getgid(void)
{
	return get_current_process()->gid;
}
void process_destroy_aspace(process_t *process)
{
	vmm_destroy_tree(process->tree);
	process->tree = NULL;
	/* TODO: Destroy the actual address space */
}
int process_attach(process_t *tracer, process_t *tracee)
{
	/* You can't attach to yourself */
	if(tracer == tracee)
		return errno = ESRCH, -1;
	/* TODO: Enforce process permitions */
	if(list_add(&tracer->tracees, tracee) < 0)
		return errno = ENOMEM, -1;
	return 0;
}
/* Finds a pid that tracer is tracing */
process_t *process_find_tracee(process_t *tracer, pid_t pid)
{
	struct list_head *list = &tracer->tracees;

	while(list && list->ptr)
	{
		process_t *tracee = list->ptr;
		if(tracee->pid == pid)
			return tracee;
		list = list->next;
	}
	return NULL;
}
