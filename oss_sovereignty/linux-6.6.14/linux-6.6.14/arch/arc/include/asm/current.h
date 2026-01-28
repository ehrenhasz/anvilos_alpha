#ifndef _ASM_ARC_CURRENT_H
#define _ASM_ARC_CURRENT_H
#ifndef __ASSEMBLY__
#ifdef CONFIG_ARC_CURR_IN_REG
register struct task_struct *curr_arc asm("gp");
#define current (curr_arc)
#else
#include <asm-generic/current.h>
#endif  
#endif  
#endif  
