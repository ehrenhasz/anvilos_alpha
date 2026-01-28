#ifndef __ASM_STACKPROTECTOR_H
#define __ASM_STACKPROTECTOR_H
#include <asm/pointer_auth.h>
extern unsigned long __stack_chk_guard;
static __always_inline void boot_init_stack_canary(void)
{
#if defined(CONFIG_STACKPROTECTOR)
	unsigned long canary = get_random_canary();
	current->stack_canary = canary;
	if (!IS_ENABLED(CONFIG_STACKPROTECTOR_PER_TASK))
		__stack_chk_guard = current->stack_canary;
#endif
	ptrauth_thread_init_kernel(current);
	ptrauth_thread_switch_kernel(current);
	ptrauth_enable();
}
#endif	 
