#ifndef _UAPI_XTENSA_TYPES_H
#define _UAPI_XTENSA_TYPES_H
#include <asm-generic/int-ll64.h>
#ifdef __ASSEMBLY__
# define __XTENSA_UL(x)		(x)
# define __XTENSA_UL_CONST(x)	x
#else
# define __XTENSA_UL(x)		((unsigned long)(x))
# define ___XTENSA_UL_CONST(x)	x##UL
# define __XTENSA_UL_CONST(x)	___XTENSA_UL_CONST(x)
#endif
#ifndef __ASSEMBLY__
#endif
#endif  
