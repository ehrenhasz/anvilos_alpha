




#ifndef _LINUX_SIMD_POWERPC_H
#define	_LINUX_SIMD_POWERPC_H

#include <linux/preempt.h>
#include <linux/export.h>
#include <linux/sched.h>
#include <asm/switch_to.h>
#include <sys/types.h>
#include <linux/version.h>

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 7, 0)
#include <asm/cpufeature.h>
#else
#include <asm/cputable.h>
#endif

#define	kfpu_allowed()			1

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 5, 0)
#ifdef	CONFIG_ALTIVEC
#define	ENABLE_KERNEL_ALTIVEC	enable_kernel_altivec();
#define	DISABLE_KERNEL_ALTIVEC	disable_kernel_altivec();
#else
#define	ENABLE_KERNEL_ALTIVEC
#define	DISABLE_KERNEL_ALTIVEC
#endif
#ifdef	CONFIG_VSX
#define	ENABLE_KERNEL_VSX	enable_kernel_vsx();
#define	DISABLE_KERNEL_VSX	disable_kernel_vsx();
#else
#define	ENABLE_KERNEL_VSX
#define	DISABLE_KERNEL_VSX
#endif
#ifdef	CONFIG_SPE
#define	ENABLE_KERNEL_SPE	enable_kernel_spe();
#define	DISABLE_KERNEL_SPE	disable_kernel_spe();
#else
#define	ENABLE_KERNEL_SPE
#define	DISABLE_KERNEL_SPE
#endif
#define	kfpu_begin()				\
	{					\
		preempt_disable();		\
		ENABLE_KERNEL_ALTIVEC		\
		ENABLE_KERNEL_VSX		\
		ENABLE_KERNEL_SPE		\
	}
#define	kfpu_end()				\
	{					\
		DISABLE_KERNEL_SPE		\
		DISABLE_KERNEL_VSX		\
		DISABLE_KERNEL_ALTIVEC		\
		preempt_enable();		\
	}
#else

#define	kfpu_begin()
#define	kfpu_end()		preempt_enable()
#endif	

#define	kfpu_init()		0
#define	kfpu_fini()		((void) 0)


#if defined(CONFIG_JUMP_LABEL_FEATURE_CHECKS) && \
    defined(HAVE_CPU_HAS_FEATURE_GPL_ONLY)
#define	cpu_has_feature(feature)	early_cpu_has_feature(feature)
#endif


static inline boolean_t
zfs_altivec_available(void)
{
	return (cpu_has_feature(CPU_FTR_ALTIVEC));
}


static inline boolean_t
zfs_vsx_available(void)
{
	return (cpu_has_feature(CPU_FTR_VSX));
}


static inline boolean_t
zfs_isa207_available(void)
{
	return (cpu_has_feature(CPU_FTR_ARCH_207S));
}

#endif 
