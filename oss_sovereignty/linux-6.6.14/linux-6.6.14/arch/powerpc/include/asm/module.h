#ifndef _ASM_POWERPC_MODULE_H
#define _ASM_POWERPC_MODULE_H
#ifdef __KERNEL__
#include <linux/list.h>
#include <asm/bug.h>
#include <asm-generic/module.h>
#ifndef __powerpc64__
struct ppc_plt_entry {
	unsigned int jump[4];
};
#endif	 
struct mod_arch_specific {
#ifdef __powerpc64__
	unsigned int stubs_section;	 
#ifdef CONFIG_PPC_KERNEL_PCREL
	unsigned int got_section;	 
	unsigned int pcpu_section;	 
#else
	unsigned int toc_section;	 
	bool toc_fixed;			 
#endif
	unsigned long start_opd;
	unsigned long end_opd;
#else  
	unsigned int core_plt_section;
	unsigned int init_plt_section;
#endif  
#ifdef CONFIG_DYNAMIC_FTRACE
	unsigned long tramp;
	unsigned long tramp_regs;
#endif
	struct list_head bug_list;
	struct bug_entry *bug_table;
	unsigned int num_bugs;
};
#ifdef __powerpc64__
#    ifdef MODULE
	asm(".section .stubs,\"ax\",@nobits; .align 3; .previous");
#        ifdef CONFIG_PPC_KERNEL_PCREL
	    asm(".section .mygot,\"a\",@nobits; .align 3; .previous");
#        endif
#    endif
#else
#    ifdef MODULE
	asm(".section .plt,\"ax\",@nobits; .align 3; .previous");
	asm(".section .init.plt,\"ax\",@nobits; .align 3; .previous");
#    endif	 
#endif
#ifdef CONFIG_DYNAMIC_FTRACE
int module_trampoline_target(struct module *mod, unsigned long trampoline,
			     unsigned long *target);
int module_finalize_ftrace(struct module *mod, const Elf_Shdr *sechdrs);
#else
static inline int module_finalize_ftrace(struct module *mod, const Elf_Shdr *sechdrs)
{
	return 0;
}
#endif
#endif  
#endif	 
