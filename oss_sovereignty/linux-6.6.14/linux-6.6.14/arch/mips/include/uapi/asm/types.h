#ifndef _UAPI_ASM_TYPES_H
#define _UAPI_ASM_TYPES_H
#ifndef __KERNEL__
# if _MIPS_SZLONG == 64 && !defined(__SANE_USERSPACE_TYPES__)
#  include <asm-generic/int-l64.h>
# else
#  include <asm-generic/int-ll64.h>
# endif
#endif
#endif  
