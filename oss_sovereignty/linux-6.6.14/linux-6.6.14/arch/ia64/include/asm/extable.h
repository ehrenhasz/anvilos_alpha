#ifndef _ASM_IA64_EXTABLE_H
#define _ASM_IA64_EXTABLE_H
#define ARCH_HAS_RELATIVE_EXTABLE
struct exception_table_entry {
	int insn;	 
	int fixup;	 
};
#endif
