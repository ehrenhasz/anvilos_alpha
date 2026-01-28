



#ifndef _FREEBSD_SIMD_AARCH64_H
#define	_FREEBSD_SIMD_AARCH64_H

#include <sys/types.h>
#include <sys/ucontext.h>
#include <machine/elf.h>
#include <machine/fpu.h>
#include <machine/md_var.h>
#include <machine/pcb.h>

#define	kfpu_allowed()		1
#define	kfpu_initialize(tsk)	do {} while (0)
#define	kfpu_begin() do {						\
	if (__predict_false(!is_fpu_kern_thread(0)))			\
		fpu_kern_enter(curthread, NULL, FPU_KERN_NOCTX);	\
} while (0)

#define	kfpu_end() do {							\
	if (__predict_false(curthread->td_pcb->pcb_fpflags & PCB_FP_NOSAVE)) \
		fpu_kern_leave(curthread, NULL);			\
} while (0)
#define	kfpu_init()		(0)
#define	kfpu_fini()		do {} while (0)


static inline boolean_t
zfs_neon_available(void)
{
	return (elf_hwcap & HWCAP_FP);
}


static inline boolean_t
zfs_sha256_available(void)
{
	return (elf_hwcap & HWCAP_SHA2);
}


static inline boolean_t
zfs_sha512_available(void)
{
	return (elf_hwcap & HWCAP_SHA512);
}

#endif 
