


#ifndef _LINUX_SIMD_H
#define	_LINUX_SIMD_H

#if defined(__x86)
#include <linux/simd_x86.h>

#elif defined(__arm__)
#include <linux/simd_arm.h>

#elif defined(__aarch64__)
#include <linux/simd_aarch64.h>

#elif defined(__powerpc__)
#include <linux/simd_powerpc.h>

#else
#define	kfpu_allowed()		0
#define	kfpu_begin()		do {} while (0)
#define	kfpu_end()		do {} while (0)
#define	kfpu_init()		0
#define	kfpu_fini()		((void) 0)

#endif
#endif 
