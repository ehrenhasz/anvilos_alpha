 

#ifndef _TOOLS_LINUX_XTENSA_SYSTEM_H
#define _TOOLS_LINUX_XTENSA_SYSTEM_H

#define mb()  ({ __asm__ __volatile__("memw" : : : "memory"); })
#define rmb() barrier()
#define wmb() mb()

#endif  
