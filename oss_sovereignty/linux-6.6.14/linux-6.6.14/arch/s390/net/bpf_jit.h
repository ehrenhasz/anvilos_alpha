#ifndef __ARCH_S390_NET_BPF_JIT_H
#define __ARCH_S390_NET_BPF_JIT_H
#ifndef __ASSEMBLY__
#include <linux/filter.h>
#include <linux/types.h>
#endif  
#define STK_SPACE_ADD	(160)
#define STK_160_UNUSED	(160 - 12 * 8)
#define STK_OFF		(STK_SPACE_ADD - STK_160_UNUSED)
#define STK_OFF_R6	(160 - 11 * 8)	 
#define STK_OFF_TCCNT	(160 - 12 * 8)	 
#endif  
