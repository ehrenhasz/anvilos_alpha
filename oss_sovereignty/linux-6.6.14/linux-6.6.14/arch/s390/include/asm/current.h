#ifndef _S390_CURRENT_H
#define _S390_CURRENT_H
#include <asm/lowcore.h>
struct task_struct;
#define current ((struct task_struct *const)S390_lowcore.current_task)
#endif  
