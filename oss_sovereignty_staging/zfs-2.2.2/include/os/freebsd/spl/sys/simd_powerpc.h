 

 

#ifndef _FREEBSD_SIMD_POWERPC_H
#define	_FREEBSD_SIMD_POWERPC_H

#include <sys/types.h>
#include <sys/cdefs.h>

#include <machine/pcb.h>
#include <machine/cpu.h>

#define	kfpu_allowed()		0
#define	kfpu_initialize(tsk)	do {} while (0)
#define	kfpu_begin()		do {} while (0)
#define	kfpu_end()		do {} while (0)
#define	kfpu_init()		(0)
#define	kfpu_fini()		do {} while (0)

 
static inline boolean_t
zfs_altivec_available(void)
{
	return ((cpu_features & PPC_FEATURE_HAS_ALTIVEC) != 0);
}

 
static inline boolean_t
zfs_vsx_available(void)
{
	return ((cpu_features & PPC_FEATURE_HAS_VSX) != 0);
}

 
static inline boolean_t
zfs_isa207_available(void)
{
	return ((cpu_features2 & PPC_FEATURE2_ARCH_2_07) != 0);
}

#endif
