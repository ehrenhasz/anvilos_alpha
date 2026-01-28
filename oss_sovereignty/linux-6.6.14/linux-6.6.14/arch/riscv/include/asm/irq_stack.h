#ifndef _ASM_RISCV_IRQ_STACK_H
#define _ASM_RISCV_IRQ_STACK_H
#include <linux/bug.h>
#include <linux/gfp.h>
#include <linux/kconfig.h>
#include <linux/vmalloc.h>
#include <linux/pgtable.h>
#include <asm/thread_info.h>
DECLARE_PER_CPU(ulong *, irq_stack_ptr);
#ifdef CONFIG_VMAP_STACK
static inline unsigned long *arch_alloc_vmap_stack(size_t stack_size, int node)
{
	void *p;
	p = __vmalloc_node(stack_size, THREAD_ALIGN, THREADINFO_GFP, node,
			__builtin_return_address(0));
	return kasan_reset_tag(p);
}
#endif  
#endif  
