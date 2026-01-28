#ifndef _ASM_RISCV_PTDUMP_H
#define _ASM_RISCV_PTDUMP_H
void ptdump_check_wx(void);
#ifdef CONFIG_DEBUG_WX
static inline void debug_checkwx(void)
{
	ptdump_check_wx();
}
#else
static inline void debug_checkwx(void)
{
}
#endif
#endif  
