

#ifndef __TOOLS_LINUX_ASM_GENERIC_BARRIER_H
#define __TOOLS_LINUX_ASM_GENERIC_BARRIER_H

#ifndef __ASSEMBLY__

#include <linux/compiler.h>



#ifndef mb
#define mb()	barrier()
#endif

#ifndef rmb
#define rmb()	mb()
#endif

#ifndef wmb
#define wmb()	mb()
#endif

#endif 
#endif 
