#ifndef __ASM_OPENRISC_UNWINDER_H
#define __ASM_OPENRISC_UNWINDER_H
void unwind_stack(void *data, unsigned long *stack,
		  void (*trace)(void *data, unsigned long addr,
				int reliable));
#endif  
