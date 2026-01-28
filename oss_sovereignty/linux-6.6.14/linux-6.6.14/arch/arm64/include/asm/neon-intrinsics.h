#ifndef __ASM_NEON_INTRINSICS_H
#define __ASM_NEON_INTRINSICS_H
#include <asm-generic/int-ll64.h>
#ifdef __INT64_TYPE__
#undef __INT64_TYPE__
#define __INT64_TYPE__		long long
#endif
#ifdef __UINT64_TYPE__
#undef __UINT64_TYPE__
#define __UINT64_TYPE__		unsigned long long
#endif
#ifndef __GENKSYMS__
#include <arm_neon.h>
#endif
#ifdef CONFIG_CC_IS_CLANG
#pragma clang diagnostic ignored "-Wincompatible-pointer-types"
#endif
#endif  
