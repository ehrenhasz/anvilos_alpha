#ifndef _ASM_IA64_MODULE_H
#define _ASM_IA64_MODULE_H
#include <asm-generic/module.h>
struct elf64_shdr;			 
struct mod_arch_specific {
	struct elf64_shdr *core_plt;	 
	struct elf64_shdr *init_plt;	 
	struct elf64_shdr *got;		 
	struct elf64_shdr *opd;		 
	struct elf64_shdr *unwind;	 
	unsigned long gp;		 
	unsigned int next_got_entry;	 
	void *core_unw_table;		 
	void *init_unw_table;		 
	void *opd_addr;			 
	unsigned long opd_size;
};
#define ARCH_SHF_SMALL	SHF_IA_64_SHORT
#endif  
