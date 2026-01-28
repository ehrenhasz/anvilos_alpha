#ifndef	_ZSTD_STRING_H
#define	_ZSTD_STRING_H
#ifdef __cplusplus
extern "C" {
#endif
#ifdef _KERNEL
#if defined(__FreeBSD__)
#include <sys/types.h>     
#include <sys/systm.h>     
#elif defined(__linux__)
#include <linux/string.h>  
#else
#error "Unsupported platform"
#endif
#else  
#include_next <string.h>
#endif  
#ifdef __cplusplus
}
#endif
#endif  
