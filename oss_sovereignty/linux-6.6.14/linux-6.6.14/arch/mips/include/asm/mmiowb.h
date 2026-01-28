#ifndef _ASM_MMIOWB_H
#define _ASM_MMIOWB_H
#include <asm/io.h>
#define mmiowb()	iobarrier_w()
#include <asm-generic/mmiowb.h>
#endif	 
