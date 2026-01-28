#ifndef _ASM_POWERPC_MMIOWB_H
#define _ASM_POWERPC_MMIOWB_H
#ifdef CONFIG_MMIOWB
#include <linux/compiler.h>
#include <asm/barrier.h>
#include <asm/paca.h>
#define arch_mmiowb_state()	(&local_paca->mmiowb_state)
#define mmiowb()		mb()
#endif  
#include <asm-generic/mmiowb.h>
#endif	 
