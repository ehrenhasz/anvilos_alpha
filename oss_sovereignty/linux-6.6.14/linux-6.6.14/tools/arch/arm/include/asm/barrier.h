#ifndef _TOOLS_LINUX_ASM_ARM_BARRIER_H
#define _TOOLS_LINUX_ASM_ARM_BARRIER_H
#define mb()		((void(*)(void))0xffff0fa0)()
#define wmb()		((void(*)(void))0xffff0fa0)()
#define rmb()		((void(*)(void))0xffff0fa0)()
#endif  
