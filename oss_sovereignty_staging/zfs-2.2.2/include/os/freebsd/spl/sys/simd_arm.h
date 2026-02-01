 

 

#ifndef _FREEBSD_SIMD_ARM_H
#define	_FREEBSD_SIMD_ARM_H

#include <sys/types.h>
#include <machine/elf.h>
#include <machine/md_var.h>

#define	kfpu_allowed()		1
#define	kfpu_initialize(tsk)	do {} while (0)
#define	kfpu_begin()		do {} while (0)
#define	kfpu_end()		do {} while (0)
#define	kfpu_init()		(0)
#define	kfpu_fini()		do {} while (0)

 
static inline boolean_t
zfs_neon_available(void)
{
	return (elf_hwcap & HWCAP_NEON);
}

 
static inline boolean_t
zfs_sha256_available(void)
{
	return (elf_hwcap2 & HWCAP2_SHA2);
}

#endif  
