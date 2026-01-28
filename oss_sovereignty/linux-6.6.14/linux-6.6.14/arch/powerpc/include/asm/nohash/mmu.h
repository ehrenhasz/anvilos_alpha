#ifndef _ASM_POWERPC_NOHASH_MMU_H_
#define _ASM_POWERPC_NOHASH_MMU_H_
#if defined(CONFIG_40x)
#include <asm/nohash/32/mmu-40x.h>
#elif defined(CONFIG_44x)
#include <asm/nohash/32/mmu-44x.h>
#elif defined(CONFIG_PPC_E500)
#include <asm/nohash/mmu-e500.h>
#elif defined (CONFIG_PPC_8xx)
#include <asm/nohash/32/mmu-8xx.h>
#endif
#endif  
