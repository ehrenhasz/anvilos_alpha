#ifndef _UAPI_SPARC_UNISTD_H
#define _UAPI_SPARC_UNISTD_H
#ifndef __32bit_syscall_numbers__
#ifndef __arch64__
#define __32bit_syscall_numbers__
#endif
#endif
#ifdef __arch64__
#include <asm/unistd_64.h>
#else
#include <asm/unistd_32.h>
#endif
#define KERN_FEATURE_MIXED_MODE_STACK	0x00000001
#endif  
