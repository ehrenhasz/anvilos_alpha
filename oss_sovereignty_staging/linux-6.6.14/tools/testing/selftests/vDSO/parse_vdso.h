 

#ifndef PARSE_VDSO_H
#define PARSE_VDSO_H

#include <stdint.h>

 
void *vdso_sym(const char *version, const char *name);
void vdso_init_from_sysinfo_ehdr(uintptr_t base);
void vdso_init_from_auxv(void *auxv);

#endif
