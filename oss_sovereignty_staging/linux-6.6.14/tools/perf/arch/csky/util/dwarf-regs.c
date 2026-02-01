



#include <stddef.h>
#include <dwarf-regs.h>

#if defined(__CSKYABIV2__)
#define CSKY_MAX_REGS 73
const char *csky_dwarf_regs_table[CSKY_MAX_REGS] = {
	 
	"%a0", "%a1", "%a2", "%a3", "%regs0", "%regs1", "%regs2", "%regs3",
	 
	"%regs4", "%regs5", "%regs6", "%regs7", "%regs8", "%regs9", "%sp",
	"%lr",
	 
	"%exregs0", "%exregs1", "%exregs2", "%exregs3", "%exregs4",
	"%exregs5", "%exregs6", "%exregs7",
	 
	"%exregs8", "%exregs9", "%exregs10", "%exregs11", "%exregs12",
	"%exregs13", "%exregs14", "%tls",
	"%pc", NULL, NULL, NULL, "%hi", "%lo", NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	"%epc",
};
#else
#define CSKY_MAX_REGS 57
const char *csky_dwarf_regs_table[CSKY_MAX_REGS] = {
	 
	"%sp", "%regs9", "%a0", "%a1", "%a2", "%a3", "%regs0", "%regs1",
	 
	"%regs2", "%regs3", "%regs4", "%regs5", "%regs6", "%regs7", "%regs8",
	"%lr",
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	"%epc",
};
#endif

const char *get_arch_regstr(unsigned int n)
{
	return (n < CSKY_MAX_REGS) ? csky_dwarf_regs_table[n] : NULL;
}
