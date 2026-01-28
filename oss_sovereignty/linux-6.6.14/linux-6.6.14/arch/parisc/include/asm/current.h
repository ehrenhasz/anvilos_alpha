#ifndef _ASM_PARISC_CURRENT_H
#define _ASM_PARISC_CURRENT_H
#ifndef __ASSEMBLY__
struct task_struct;
static __always_inline struct task_struct *get_current(void)
{
	struct task_struct *ts;
	asm( "mfctl %%cr30,%0" : "=r" (ts) );
	return ts;
}
#define current get_current()
#endif  
#endif  
