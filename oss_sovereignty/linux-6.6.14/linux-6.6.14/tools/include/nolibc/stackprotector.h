#ifndef _NOLIBC_STACKPROTECTOR_H
#define _NOLIBC_STACKPROTECTOR_H
#include "compiler.h"
#if defined(_NOLIBC_STACKPROTECTOR)
#include "sys.h"
#include "stdlib.h"
__attribute__((weak,noreturn,section(".text.nolibc_stack_chk")))
void __stack_chk_fail(void)
{
	pid_t pid;
	my_syscall3(__NR_write, STDERR_FILENO, "!!Stack smashing detected!!\n", 28);
	pid = my_syscall0(__NR_getpid);
	my_syscall2(__NR_kill, pid, SIGABRT);
	for (;;);
}
__attribute__((weak,noreturn,section(".text.nolibc_stack_chk")))
void __stack_chk_fail_local(void)
{
	__stack_chk_fail();
}
__attribute__((weak,section(".data.nolibc_stack_chk")))
uintptr_t __stack_chk_guard;
static __no_stack_protector void __stack_chk_init(void)
{
	my_syscall3(__NR_getrandom, &__stack_chk_guard, sizeof(__stack_chk_guard), 0);
	if (__stack_chk_guard != (uintptr_t) &__stack_chk_guard)
		__stack_chk_guard ^= (uintptr_t) &__stack_chk_guard;
}
#else  
static void __stack_chk_init(void) {}
#endif  
#endif  
