#ifndef _XTENSA_MXREGS_H
#define _XTENSA_MXREGS_H
#define MIROUT(irq)	(0x000 + (irq))
#define MIPICAUSE(cpu)	(0x100 + (cpu))
#define MIPISET(cause)	(0x140 + (cause))
#define MIENG		0x180
#define MIENGSET	0x184
#define MIASG		0x188	 
#define MIASGSET	0x18c	 
#define MIPIPART	0x190
#define SYSCFGID	0x1a0
#define MPSCORE		0x200
#define CCON		0x220
#endif  
