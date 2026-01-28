#ifndef PERVASIVE_H
#define PERVASIVE_H
extern void cbe_pervasive_init(void);
#ifdef CONFIG_PPC_IBM_CELL_RESETBUTTON
extern int cbe_sysreset_hack(void);
#else
static inline int cbe_sysreset_hack(void)
{
	return 1;
}
#endif  
#endif
