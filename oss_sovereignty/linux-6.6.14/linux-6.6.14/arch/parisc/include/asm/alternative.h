#ifndef __ASM_PARISC_ALTERNATIVE_H
#define __ASM_PARISC_ALTERNATIVE_H
#define ALT_COND_ALWAYS		0x80	 
#define ALT_COND_NO_SMP		0x01	 
#define ALT_COND_NO_DCACHE	0x02	 
#define ALT_COND_NO_ICACHE	0x04	 
#define ALT_COND_NO_SPLIT_TLB	0x08	 
#define ALT_COND_NO_IOC_FDC	0x10	 
#define ALT_COND_RUN_ON_QEMU	0x20	 
#define INSN_PxTLB	0x02		 
#define INSN_NOP	0x08000240	 
#ifndef __ASSEMBLY__
#include <linux/init.h>
#include <linux/types.h>
#include <linux/stddef.h>
#include <linux/stringify.h>
struct alt_instr {
	s32 orig_offset;	 
	s16 len;		 
	u16 cond;		 
	u32 replacement;	 
} __packed;
void set_kernel_text_rw(int enable_read_write);
void apply_alternatives_all(void);
void apply_alternatives(struct alt_instr *start, struct alt_instr *end,
	const char *module_name);
#define ALTERNATIVE(cond, replacement)		"!0:"	\
	".section .altinstructions, \"a\"	!"	\
	".align 4				!"	\
	".word (0b-4-.)				!"	\
	".hword 1, " __stringify(cond) "	!"	\
	".word " __stringify(replacement) "	!"	\
	".previous"
#else
#define ALTERNATIVE(from, to, cond, replacement)\
	.section .altinstructions, "a"	!	\
	.align 4			!	\
	.word (from - .)		!	\
	.hword (to - from)/4, cond	!	\
	.word replacement		!	\
	.previous
#define ALTERNATIVE_CODE(from, num_instructions, cond, new_instr_ptr)\
	.section .altinstructions, "a"	!	\
	.align 4			!	\
	.word (from - .)		!	\
	.hword -num_instructions, cond	!	\
	.word (new_instr_ptr - .)	!	\
	.previous
#endif   
#endif  
