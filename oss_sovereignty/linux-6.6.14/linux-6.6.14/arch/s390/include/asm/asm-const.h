#ifndef _ASM_S390_ASM_CONST_H
#define _ASM_S390_ASM_CONST_H
#ifdef __ASSEMBLY__
#  define stringify_in_c(...)	__VA_ARGS__
#else
#  define __stringify_in_c(...)	#__VA_ARGS__
#  define stringify_in_c(...)	__stringify_in_c(__VA_ARGS__) " "
#endif
#endif  
