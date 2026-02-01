 

 

#ifndef	_SYS_ISA_DEFS_H
#define	_SYS_ISA_DEFS_H
#include <sys/endian.h>

 

#ifdef	__cplusplus
extern "C" {
#endif

 
#if defined(__x86_64) || defined(__amd64)

#if !defined(__amd64)
#define	__amd64		 
#endif

#if !defined(__x86)
#define	__x86
#endif

 
#if !defined(_LP64)
#error "_LP64 not defined"
#endif
#define	_SUNOS_VTOC_16

 
#elif defined(__i386) || defined(__i386__)

#if !defined(__i386)
#define	__i386
#endif

#if !defined(__x86)
#define	__x86
#endif

 
#if !defined(_ILP32)
#define	_ILP32
#endif
#define	_SUNOS_VTOC_16

#elif defined(__aarch64__)

 
#if !defined(_LP64)
#error "_LP64 not defined"
#endif
#define	_SUNOS_VTOC_16

#elif defined(__riscv)

 
#if !defined(_LP64)
#define	_LP64
#endif
#define	_SUNOS_VTOC_16

#elif defined(__arm__)

 
#if !defined(_ILP32)
#define	_ILP32
#endif
#define	_SUNOS_VTOC_16

#elif defined(__mips__)

#if defined(__mips_n64)
 
#if !defined(_LP64)
#error "_LP64 not defined"
#endif
#else
 
#if !defined(_ILP32)
#define	_ILP32
#endif
#endif
#define	_SUNOS_VTOC_16

#elif defined(__powerpc__)

#if !defined(__powerpc)
#define	__powerpc
#endif

#define	_SUNOS_VTOC_16	1

 
 
#elif defined(__sparc) || defined(__sparcv9) || defined(__sparc__)
#if !defined(__sparc)
#define	__sparc
#endif

 
#if defined(__sparcv8) && defined(__sparcv9)
#error	"SPARC Versions 8 and 9 are mutually exclusive choices"
#endif

 
#if !defined(__sparcv9) && !defined(__sparcv8)
#define	__sparcv8
#endif

 
#define	_SUNOS_VTOC_8

 
#if defined(__sparcv8)

 
#define	_ILP32

 
#elif defined(__sparcv9)

 
#if !defined(_LP64)
#error "_LP64 not defined"
#endif

#else
#error	"unknown SPARC version"
#endif

 
#else
#error "ISA not supported"
#endif

#if defined(_ILP32) && defined(_LP64)
#error "Both _ILP32 and _LP64 are defined"
#endif

#if BYTE_ORDER == _BIG_ENDIAN
#define	_ZFS_BIG_ENDIAN
#elif BYTE_ORDER == _LITTLE_ENDIAN
#define	_ZFS_LITTLE_ENDIAN
#else
#error "unknown byte order"
#endif

#ifdef	__cplusplus
}
#endif

#endif	 
