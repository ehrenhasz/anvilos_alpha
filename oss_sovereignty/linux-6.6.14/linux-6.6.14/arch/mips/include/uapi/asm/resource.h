#ifndef _ASM_RESOURCE_H
#define _ASM_RESOURCE_H
#define RLIMIT_NOFILE		5	 
#define RLIMIT_AS		6	 
#define RLIMIT_RSS		7	 
#define RLIMIT_NPROC		8	 
#define RLIMIT_MEMLOCK		9	 
#ifndef __mips64
# define RLIM_INFINITY		0x7fffffffUL
#endif
#include <asm-generic/resource.h>
#endif  
