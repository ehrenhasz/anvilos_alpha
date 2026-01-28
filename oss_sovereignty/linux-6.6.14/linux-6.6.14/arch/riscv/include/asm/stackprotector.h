#ifndef _ASM_RISCV_STACKPROTECTOR_H
#define _ASM_RISCV_STACKPROTECTOR_H
extern unsigned long __stack_chk_guard;
static __always_inline void boot_init_stack_canary(void)
{
	unsigned long canary = get_random_canary();
	current->stack_canary = canary;
	if (!IS_ENABLED(CONFIG_STACKPROTECTOR_PER_TASK))
		__stack_chk_guard = current->stack_canary;
}
#endif  
