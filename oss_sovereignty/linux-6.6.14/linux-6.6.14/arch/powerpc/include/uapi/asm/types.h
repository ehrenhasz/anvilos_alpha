#ifndef _UAPI_ASM_POWERPC_TYPES_H
#define _UAPI_ASM_POWERPC_TYPES_H
#if !defined(__SANE_USERSPACE_TYPES__) && defined(__powerpc64__) && !defined(__KERNEL__)
# include <asm-generic/int-l64.h>
#else
# include <asm-generic/int-ll64.h>
#endif
#ifndef __ASSEMBLY__
typedef struct {
	__u32 u[4];
} __attribute__((aligned(16))) __vector128;
#endif  
#endif  
