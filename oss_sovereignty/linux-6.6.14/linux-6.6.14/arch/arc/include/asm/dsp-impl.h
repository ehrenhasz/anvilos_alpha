#ifndef __ASM_ARC_DSP_IMPL_H
#define __ASM_ARC_DSP_IMPL_H
#include <asm/dsp.h>
#define DSP_CTRL_DISABLED_ALL		0
#ifdef __ASSEMBLY__
.macro DSP_EARLY_INIT
#ifdef CONFIG_ISA_ARCV2
	lr	r5, [ARC_AUX_DSP_BUILD]
	bmsk	r5, r5, 7
	breq    r5, 0, 1f
	mov	r5, DSP_CTRL_DISABLED_ALL
	sr	r5, [ARC_AUX_DSP_CTRL]
1:
#endif
.endm
.macro DSP_SAVE_REGFILE_IRQ
#if defined(CONFIG_ARC_DSP_KERNEL)
	mov	r10, DSP_CTRL_DISABLED_ALL
	sr	r10, [ARC_AUX_DSP_CTRL]
#elif defined(CONFIG_ARC_DSP_SAVE_RESTORE_REGS)
	mov	r10, DSP_CTRL_DISABLED_ALL
	aex	r10, [ARC_AUX_DSP_CTRL]
	st	r10, [sp, PT_DSP_CTRL]
#endif
.endm
.macro DSP_RESTORE_REGFILE_IRQ
#if defined(CONFIG_ARC_DSP_SAVE_RESTORE_REGS)
	ld	r10, [sp, PT_DSP_CTRL]
	sr	r10, [ARC_AUX_DSP_CTRL]
#endif
.endm
#else  
#include <linux/sched.h>
#include <asm/asserts.h>
#include <asm/switch_to.h>
#ifdef CONFIG_ARC_DSP_SAVE_RESTORE_REGS
#define AUX_SAVE_RESTORE(_saveto, _readfrom, _offt, _aux)		\
do {									\
	long unsigned int _scratch;					\
									\
	__asm__ __volatile__(						\
		"ld	%0, [%2, %4]			\n"		\
		"aex	%0, [%3]			\n"		\
		"st	%0, [%1, %4]			\n"		\
		:							\
		  "=&r" (_scratch)	 	\
		:							\
		   "r" (_saveto),					\
		   "r" (_readfrom),					\
		   "Ir" (_aux),						\
		   "Ir" (_offt)						\
		:							\
		  "memory"						\
	);								\
} while (0)
#define DSP_AUX_SAVE_RESTORE(_saveto, _readfrom, _aux)			\
	AUX_SAVE_RESTORE(_saveto, _readfrom,				\
		offsetof(struct dsp_callee_regs, _aux),			\
		ARC_AUX_##_aux)
static inline void dsp_save_restore(struct task_struct *prev,
					struct task_struct *next)
{
	long unsigned int *saveto = &prev->thread.dsp.ACC0_GLO;
	long unsigned int *readfrom = &next->thread.dsp.ACC0_GLO;
	DSP_AUX_SAVE_RESTORE(saveto, readfrom, ACC0_GLO);
	DSP_AUX_SAVE_RESTORE(saveto, readfrom, ACC0_GHI);
	DSP_AUX_SAVE_RESTORE(saveto, readfrom, DSP_BFLY0);
	DSP_AUX_SAVE_RESTORE(saveto, readfrom, DSP_FFT_CTRL);
#ifdef CONFIG_ARC_DSP_AGU_USERSPACE
	DSP_AUX_SAVE_RESTORE(saveto, readfrom, AGU_AP0);
	DSP_AUX_SAVE_RESTORE(saveto, readfrom, AGU_AP1);
	DSP_AUX_SAVE_RESTORE(saveto, readfrom, AGU_AP2);
	DSP_AUX_SAVE_RESTORE(saveto, readfrom, AGU_AP3);
	DSP_AUX_SAVE_RESTORE(saveto, readfrom, AGU_OS0);
	DSP_AUX_SAVE_RESTORE(saveto, readfrom, AGU_OS1);
	DSP_AUX_SAVE_RESTORE(saveto, readfrom, AGU_MOD0);
	DSP_AUX_SAVE_RESTORE(saveto, readfrom, AGU_MOD1);
	DSP_AUX_SAVE_RESTORE(saveto, readfrom, AGU_MOD2);
	DSP_AUX_SAVE_RESTORE(saveto, readfrom, AGU_MOD3);
#endif  
}
#else  
#define dsp_save_restore(p, n)
#endif  
static inline bool dsp_exist(void)
{
	struct bcr_generic bcr;
	READ_BCR(ARC_AUX_DSP_BUILD, bcr);
	return !!bcr.ver;
}
static inline bool agu_exist(void)
{
	struct bcr_generic bcr;
	READ_BCR(ARC_AUX_AGU_BUILD, bcr);
	return !!bcr.ver;
}
static inline void dsp_config_check(void)
{
	CHK_OPT_STRICT(CONFIG_ARC_DSP_HANDLED, dsp_exist());
	CHK_OPT_WEAK(CONFIG_ARC_DSP_AGU_USERSPACE, agu_exist());
}
#endif  
#endif  
