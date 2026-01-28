#ifndef __HEXAGON_KGDB_H__
#define __HEXAGON_KGDB_H__
#define BREAK_INSTR_SIZE 4
#define CACHE_FLUSH_IS_SAFE   1
#define BUFMAX       ((NUMREGBYTES * 2) + 512)
static inline void arch_kgdb_breakpoint(void)
{
	asm("trap0(#0xDB)");
}
#define DBG_USER_REGS 42
#define DBG_MAX_REG_NUM (DBG_USER_REGS + 8)
#define NUMREGBYTES  (DBG_MAX_REG_NUM*4)
#endif  
