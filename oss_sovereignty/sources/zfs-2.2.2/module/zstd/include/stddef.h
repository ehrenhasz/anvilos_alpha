



#ifndef	_ZSTD_STDDEF_H
#define	_ZSTD_STDDEF_H

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _KERNEL

#if defined(__FreeBSD__)
#include <sys/types.h>
#elif defined(__linux__)
#include <linux/types.h>
#else
#error "Unsupported platform"
#endif

#else 
#include_next <stddef.h>
#endif 

#ifdef __cplusplus
}
#endif

#endif 
