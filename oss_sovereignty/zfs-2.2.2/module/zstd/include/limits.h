 

 

#ifndef	_ZSTD_LIMITS_H
#define	_ZSTD_LIMITS_H

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _KERNEL

#if defined(__FreeBSD__)
#include <sys/limits.h>
#elif defined(__linux__)
#include <linux/limits.h>
#include <linux/kernel.h>
#else
#error "Unsupported platform"
#endif

#else  
#include_next <limits.h>
#endif  

#ifdef __cplusplus
}
#endif

#endif  
