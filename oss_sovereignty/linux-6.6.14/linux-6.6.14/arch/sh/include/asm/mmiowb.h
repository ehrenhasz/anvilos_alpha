#ifndef __ASM_SH_MMIOWB_H
#define __ASM_SH_MMIOWB_H
#include <asm/barrier.h>
#define mmiowb()			wmb()
#include <asm-generic/mmiowb.h>
#endif	 
