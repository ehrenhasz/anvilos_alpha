#ifndef _ASM_IA64_MMIOWB_H
#define _ASM_IA64_MMIOWB_H
#define mmiowb()	ia64_mfa()
#include <asm-generic/mmiowb.h>
#endif	 
