#ifdef __KERNEL__
#ifndef __POWERPC_KGDB_H__
#define __POWERPC_KGDB_H__
#ifndef __ASSEMBLY__
#define BREAK_INSTR_SIZE	4
#define BUFMAX			((NUMREGBYTES * 2) + 512)
#define OUTBUFMAX		((NUMREGBYTES * 2) + 512)
#define BREAK_INSTR		0x7d821008	 
static inline void arch_kgdb_breakpoint(void)
{
	asm(stringify_in_c(.long BREAK_INSTR));
}
#define CACHE_FLUSH_IS_SAFE	1
#define DBG_MAX_REG_NUM     70
#ifdef CONFIG_PPC64
#define NUMREGBYTES		((68 * 8) + (3 * 4))
#define NUMCRITREGBYTES		184
#else  
#ifndef CONFIG_PPC_E500
#define MAXREG			(PT_FPSCR+1)
#else
#define MAXREG                 ((32*2)+6+2+1)
#endif
#define NUMREGBYTES		(MAXREG * sizeof(int))
#define NUMCRITREGBYTES		(23 * sizeof(int))
#endif  
#endif  
#endif  
#endif  
