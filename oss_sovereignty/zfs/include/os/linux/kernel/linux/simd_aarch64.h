#ifndef _LINUX_SIMD_AARCH64_H
#define	_LINUX_SIMD_AARCH64_H
#include <sys/types.h>
#include <asm/neon.h>
#include <asm/elf.h>
#include <asm/hwcap.h>
#include <linux/version.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 17, 0)
#include <asm/sysreg.h>
#else
#define	sys_reg(op0, op1, crn, crm, op2) ( \
	((op0) << Op0_shift) | \
	((op1) << Op1_shift) | \
	((crn) << CRn_shift) | \
	((crm) << CRm_shift) | \
	((op2) << Op2_shift))
#endif
#define	ID_AA64PFR0_EL1		sys_reg(3, 0, 0, 1, 0)
#define	ID_AA64ISAR0_EL1	sys_reg(3, 0, 0, 6, 0)
#define	kfpu_allowed()		1
#define	kfpu_begin()		kernel_neon_begin()
#define	kfpu_end()		kernel_neon_end()
#define	kfpu_init()		(0)
#define	kfpu_fini()		do {} while (0)
#define	get_ftr(id) {				\
	unsigned long __val;			\
	asm("mrs %0, "#id : "=r" (__val));	\
	__val;					\
}
static inline boolean_t
zfs_neon_available(void)
{
	unsigned long ftr = ((get_ftr(ID_AA64PFR0_EL1)) >> 16) & 0xf;
	return (ftr == 0 || ftr == 1);
}
static inline boolean_t
zfs_sha256_available(void)
{
	unsigned long ftr = ((get_ftr(ID_AA64ISAR0_EL1)) >> 12) & 0x3;
	return (ftr & 0x1);
}
static inline boolean_t
zfs_sha512_available(void)
{
	unsigned long ftr = ((get_ftr(ID_AA64ISAR0_EL1)) >> 12) & 0x3;
	return (ftr & 0x2);
}
#endif  
