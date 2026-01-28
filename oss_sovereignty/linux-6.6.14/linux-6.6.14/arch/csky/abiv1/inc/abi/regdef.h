#ifndef __ASM_CSKY_REGDEF_H
#define __ASM_CSKY_REGDEF_H
#ifdef __ASSEMBLY__
#define syscallid	r1
#else
#define syscallid	"r1"
#endif
#define regs_syscallid(regs) regs->regs[9]
#define regs_fp(regs) regs->regs[2]
#define DEFAULT_PSR_VALUE	0x8f000000
#define SYSTRACE_SAVENUM	2
#define TRAP0_SIZE		2
#endif  
