#ifndef _ASM_RISCV_MMIOWB_H
#define _ASM_RISCV_MMIOWB_H
#define mmiowb()	__asm__ __volatile__ ("fence o,w" : : : "memory");
#include <linux/smp.h>
#include <asm-generic/mmiowb.h>
#endif	 
