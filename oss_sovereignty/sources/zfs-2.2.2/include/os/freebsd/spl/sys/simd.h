

#ifndef _FREEBSD_SIMD_H
#define	_FREEBSD_SIMD_H

#if defined(__amd64__) || defined(__i386__)
#include <sys/simd_x86.h>

#elif defined(__arm__)
#include <sys/simd_arm.h>

#elif defined(__aarch64__)
#include <sys/simd_aarch64.h>

#elif defined(__powerpc__)
#include <sys/simd_powerpc.h>

#else
#define	kfpu_allowed()		0
#define	kfpu_initialize(tsk)	do {} while (0)
#define	kfpu_begin()		do {} while (0)
#define	kfpu_end()		do {} while (0)
#define	kfpu_init()		(0)
#define	kfpu_fini()		do {} while (0)
#endif

#endif
