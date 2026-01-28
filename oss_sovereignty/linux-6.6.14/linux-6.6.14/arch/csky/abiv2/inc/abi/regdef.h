#ifndef __ASM_CSKY_REGDEF_H
#define __ASM_CSKY_REGDEF_H
#ifdef __ASSEMBLY__
#define syscallid	r7
#else
#define syscallid	"r7"
#endif
#define regs_syscallid(regs) regs->regs[3]
#define regs_fp(regs) regs->regs[4]
#define DEFAULT_PSR_VALUE	0x80000200
#define SYSTRACE_SAVENUM	5
#define TRAP0_SIZE		4
#endif  
