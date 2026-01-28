#ifndef _ASM_HEXAGON_TRAPS_H
#define _ASM_HEXAGON_TRAPS_H
#include <asm/registers.h>
extern int die(const char *str, struct pt_regs *regs, long err);
extern int die_if_kernel(char *str, struct pt_regs *regs, long err);
#endif  
