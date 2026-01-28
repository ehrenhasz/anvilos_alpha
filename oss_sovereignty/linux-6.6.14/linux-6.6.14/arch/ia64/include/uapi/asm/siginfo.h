#ifndef _UAPI_ASM_IA64_SIGINFO_H
#define _UAPI_ASM_IA64_SIGINFO_H
#include <asm-generic/siginfo.h>
#define si_imm		_sifields._sigfault._imm	 
#define si_flags	_sifields._sigfault._flags
#define si_isr		_sifields._sigfault._isr
#define __ISR_VALID_BIT	0
#define __ISR_VALID	(1 << __ISR_VALID_BIT)
#endif  
