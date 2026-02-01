 

 

#ifndef	_ZSTD_STDINT_H
#define	_ZSTD_STDINT_H

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _KERNEL

#if defined(__FreeBSD__)
#include <sys/stdint.h>
#elif defined(__linux__)
#include <linux/types.h>
#else
#error "Unsupported platform"
#endif

#else  
#include_next <stdint.h>
#endif  

#ifdef __cplusplus
}
#endif

#endif  
