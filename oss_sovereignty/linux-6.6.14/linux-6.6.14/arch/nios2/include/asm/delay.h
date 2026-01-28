#ifndef _ASM_NIOS2_DELAY_H
#define _ASM_NIOS2_DELAY_H
#include <asm-generic/delay.h>
extern void __bad_udelay(void);
extern void __bad_ndelay(void);
extern unsigned long loops_per_jiffy;
#endif  
