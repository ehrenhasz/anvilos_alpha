





#ifndef _LINUX_SIMD_ARM_H
#define	_LINUX_SIMD_ARM_H

#include <sys/types.h>
#include <asm/neon.h>
#include <asm/elf.h>
#include <asm/hwcap.h>

#define	kfpu_allowed()		1
#define	kfpu_begin()		kernel_neon_begin()
#define	kfpu_end()		kernel_neon_end()
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
