#ifndef _UAPI_ASM_SIGINFO_H
#define _UAPI_ASM_SIGINFO_H
#define __ARCH_SIGEV_PREAMBLE_SIZE (sizeof(long) + 2*sizeof(int))
#define __ARCH_HAS_SWAPPED_SIGINFO
#include <asm-generic/siginfo.h>
#undef SI_ASYNCIO
#undef SI_TIMER
#undef SI_MESGQ
#define SI_ASYNCIO	-2	 
#define SI_TIMER	-3	 
#define SI_MESGQ	-4	 
#endif  
