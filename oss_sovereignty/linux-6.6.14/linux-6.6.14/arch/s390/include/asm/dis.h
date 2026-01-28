#ifndef __ASM_S390_DIS_H__
#define __ASM_S390_DIS_H__
#include <asm/dis-defs.h>
static inline int insn_length(unsigned char code)
{
	return ((((int) code + 64) >> 7) + 1) << 1;
}
struct pt_regs;
void show_code(struct pt_regs *regs);
void print_fn_code(unsigned char *code, unsigned long len);
struct s390_insn *find_insn(unsigned char *code);
static inline int is_known_insn(unsigned char *code)
{
	return !!find_insn(code);
}
#endif  
