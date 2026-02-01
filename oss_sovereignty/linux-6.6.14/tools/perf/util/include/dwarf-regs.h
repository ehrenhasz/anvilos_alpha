 
#ifndef _PERF_DWARF_REGS_H_
#define _PERF_DWARF_REGS_H_

#ifdef HAVE_DWARF_SUPPORT
const char *get_arch_regstr(unsigned int n);
 
const char *get_dwarf_regstr(unsigned int n, unsigned int machine);
#endif

#ifdef HAVE_ARCH_REGS_QUERY_REGISTER_OFFSET
 
int regs_query_register_offset(const char *name);
#endif
#endif
