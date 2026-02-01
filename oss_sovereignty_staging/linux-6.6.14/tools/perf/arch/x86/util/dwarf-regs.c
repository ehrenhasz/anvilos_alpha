
 

#include <stddef.h>
#include <errno.h>  
#include <string.h>  
#include <linux/ptrace.h>  
#include <linux/kernel.h>  
#include <dwarf-regs.h>

 

struct pt_regs_offset {
	const char *name;
	int offset;
};

#define REG_OFFSET_END {.name = NULL, .offset = 0}

#ifdef __x86_64__
# define REG_OFFSET_NAME_64(n, r) {.name = n, .offset = offsetof(struct pt_regs, r)}
# define REG_OFFSET_NAME_32(n, r) {.name = n, .offset = -1}
#else
# define REG_OFFSET_NAME_64(n, r) {.name = n, .offset = -1}
# define REG_OFFSET_NAME_32(n, r) {.name = n, .offset = offsetof(struct pt_regs, r)}
#endif

 
#ifndef __x86_64__
static const struct pt_regs_offset x86_32_regoffset_table[] = {
	REG_OFFSET_NAME_32("%ax",	eax),
	REG_OFFSET_NAME_32("%cx",	ecx),
	REG_OFFSET_NAME_32("%dx",	edx),
	REG_OFFSET_NAME_32("%bx",	ebx),
	REG_OFFSET_NAME_32("$stack",	esp),	 
	REG_OFFSET_NAME_32("%bp",	ebp),
	REG_OFFSET_NAME_32("%si",	esi),
	REG_OFFSET_NAME_32("%di",	edi),
	REG_OFFSET_END,
};

#define regoffset_table x86_32_regoffset_table
#else
static const struct pt_regs_offset x86_64_regoffset_table[] = {
	REG_OFFSET_NAME_64("%ax",	rax),
	REG_OFFSET_NAME_64("%dx",	rdx),
	REG_OFFSET_NAME_64("%cx",	rcx),
	REG_OFFSET_NAME_64("%bx",	rbx),
	REG_OFFSET_NAME_64("%si",	rsi),
	REG_OFFSET_NAME_64("%di",	rdi),
	REG_OFFSET_NAME_64("%bp",	rbp),
	REG_OFFSET_NAME_64("%sp",	rsp),
	REG_OFFSET_NAME_64("%r8",	r8),
	REG_OFFSET_NAME_64("%r9",	r9),
	REG_OFFSET_NAME_64("%r10",	r10),
	REG_OFFSET_NAME_64("%r11",	r11),
	REG_OFFSET_NAME_64("%r12",	r12),
	REG_OFFSET_NAME_64("%r13",	r13),
	REG_OFFSET_NAME_64("%r14",	r14),
	REG_OFFSET_NAME_64("%r15",	r15),
	REG_OFFSET_END,
};

#define regoffset_table x86_64_regoffset_table
#endif

 
#define ARCH_MAX_REGS ((sizeof(regoffset_table) / sizeof(regoffset_table[0])) - 1)

 
const char *get_arch_regstr(unsigned int n)
{
	return (n < ARCH_MAX_REGS) ? regoffset_table[n].name : NULL;
}

 
 
int regs_query_register_offset(const char *name)
{
	const struct pt_regs_offset *roff;
	for (roff = regoffset_table; roff->name != NULL; roff++)
		if (!strcmp(roff->name, name))
			return roff->offset;
	return -EINVAL;
}
