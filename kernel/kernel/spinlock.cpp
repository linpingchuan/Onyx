/* Copyright 2016 Pedro Falcato

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

http ://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/
#include <kernel/spinlock.h>
#include <stdio.h>
#include <kernel/compiler.h>
void acquire(spinlock_t* lock)
{
	if(lock->lock == 1)
	{
		wait(lock);
	}
	__sync_lock_test_and_set(&lock->lock,1);
}
void release(spinlock_t* lock)
{
	__sync_lock_release(&lock->lock);
}
void wait(spinlock_t* lock)
{
	while(lock->lock == 1)
	{
		asm volatile("int $0x50");
	}
}