#ifndef _ARCH_POWERPC_EXTABLE_H
#define _ARCH_POWERPC_EXTABLE_H
#define ARCH_HAS_RELATIVE_EXTABLE
#ifndef __ASSEMBLY__
struct exception_table_entry {
	int insn;
	int fixup;
};
static inline unsigned long extable_fixup(const struct exception_table_entry *x)
{
	return (unsigned long)&x->fixup + x->fixup;
}
#endif
#define EX_TABLE(_fault, _target)		\
	stringify_in_c(.section __ex_table,"a";)\
	stringify_in_c(.balign 4;)		\
	stringify_in_c(.long (_fault) - . ;)	\
	stringify_in_c(.long (_target) - . ;)	\
	stringify_in_c(.previous)
#endif
