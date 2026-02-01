
 

#include <errno.h>
#include <stddef.h>
#include <stdlib.h>
#include <linux/kernel.h>
#include <asm/ptrace.h>
#include <string.h>
#include <dwarf-regs.h>
#include "dwarf-regs-table.h"

const char *get_arch_regstr(unsigned int n)
{
	return (n >= ARRAY_SIZE(s390_dwarf_regs)) ? NULL : s390_dwarf_regs[n];
}

 
int regs_query_register_offset(const char *name)
{
	unsigned long gpr;

	if (!name || strncmp(name, "%r", 2))
		return -EINVAL;

	errno = 0;
	gpr = strtoul(name + 2, NULL, 10);
	if (errno || gpr >= 16)
		return -EINVAL;

	return offsetof(user_pt_regs, gprs) + 8 * gpr;
}
