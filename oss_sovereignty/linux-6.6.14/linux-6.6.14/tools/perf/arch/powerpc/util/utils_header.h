#ifndef __PERF_UTIL_HEADER_H
#define __PERF_UTIL_HEADER_H
#include <linux/stringify.h>
#define mfspr(rn)       ({unsigned long rval; \
			asm volatile("mfspr %0," __stringify(rn) \
				: "=r" (rval)); rval; })
#define SPRN_PVR        0x11F    
#define PVR_VER(pvr)    (((pvr) >>  16) & 0xFFFF)  
#define PVR_REV(pvr)    (((pvr) >>   0) & 0xFFFF)  
#endif  
