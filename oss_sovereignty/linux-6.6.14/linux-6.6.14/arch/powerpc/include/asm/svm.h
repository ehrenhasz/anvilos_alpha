#ifndef _ASM_POWERPC_SVM_H
#define _ASM_POWERPC_SVM_H
#ifdef CONFIG_PPC_SVM
#include <asm/reg.h>
static inline bool is_secure_guest(void)
{
	return mfmsr() & MSR_S;
}
void dtl_cache_ctor(void *addr);
#define get_dtl_cache_ctor()	(is_secure_guest() ? dtl_cache_ctor : NULL)
#else  
static inline bool is_secure_guest(void)
{
	return false;
}
#define get_dtl_cache_ctor() NULL
#endif  
#endif  
