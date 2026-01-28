#ifndef _ASM_PROCESSOR_H
#define _ASM_PROCESSOR_H
#include <linux/atomic.h>
#include <linux/cpumask.h>
#include <linux/sizes.h>
#include <linux/threads.h>
#include <asm/cachectl.h>
#include <asm/cpu.h>
#include <asm/cpu-info.h>
#include <asm/dsemul.h>
#include <asm/mipsregs.h>
#include <asm/prefetch.h>
#include <asm/vdso/processor.h>
extern unsigned int vced_count, vcei_count;
extern int arch_dup_task_struct(struct task_struct *dst, struct task_struct *src);
#ifdef CONFIG_32BIT
#define TASK_SIZE	0x80000000UL
#define STACK_TOP_MAX	TASK_SIZE
#define TASK_IS_32BIT_ADDR 1
#endif
#ifdef CONFIG_64BIT
#define TASK_SIZE32	0x7fff8000UL
#ifdef CONFIG_MIPS_VA_BITS_48
#define TASK_SIZE64     (0x1UL << ((cpu_data[0].vmbits>48)?48:cpu_data[0].vmbits))
#else
#define TASK_SIZE64     0x10000000000UL
#endif
#define TASK_SIZE (test_thread_flag(TIF_32BIT_ADDR) ? TASK_SIZE32 : TASK_SIZE64)
#define STACK_TOP_MAX	TASK_SIZE64
#define TASK_SIZE_OF(tsk)						\
	(test_tsk_thread_flag(tsk, TIF_32BIT_ADDR) ? TASK_SIZE32 : TASK_SIZE64)
#define TASK_IS_32BIT_ADDR test_thread_flag(TIF_32BIT_ADDR)
#endif
#define VDSO_RANDOMIZE_SIZE	(TASK_IS_32BIT_ADDR ? SZ_1M : SZ_64M)
extern unsigned long mips_stack_top(void);
#define STACK_TOP		mips_stack_top()
#define TASK_UNMAPPED_BASE PAGE_ALIGN(TASK_SIZE / 3)
#define NUM_FPU_REGS	32
#ifdef CONFIG_CPU_HAS_MSA
# define FPU_REG_WIDTH	128
#else
# define FPU_REG_WIDTH	64
#endif
union fpureg {
	__u32	val32[FPU_REG_WIDTH / 32];
	__u64	val64[FPU_REG_WIDTH / 64];
};
#ifdef CONFIG_CPU_LITTLE_ENDIAN
# define FPR_IDX(width, idx)	(idx)
#else
# define FPR_IDX(width, idx)	((idx) ^ ((64 / (width)) - 1))
#endif
#define BUILD_FPR_ACCESS(width) \
static inline u##width get_fpr##width(union fpureg *fpr, unsigned idx)	\
{									\
	return fpr->val##width[FPR_IDX(width, idx)];			\
}									\
									\
static inline void set_fpr##width(union fpureg *fpr, unsigned idx,	\
				  u##width val)				\
{									\
	fpr->val##width[FPR_IDX(width, idx)] = val;			\
}
BUILD_FPR_ACCESS(32)
BUILD_FPR_ACCESS(64)
struct mips_fpu_struct {
	union fpureg	fpr[NUM_FPU_REGS];
	unsigned int	fcr31;
	unsigned int	msacsr;
};
#define NUM_DSP_REGS   6
typedef unsigned long dspreg_t;
struct mips_dsp_state {
	dspreg_t	dspr[NUM_DSP_REGS];
	unsigned int	dspcontrol;
};
#define INIT_CPUMASK { \
	{0,} \
}
struct mips3264_watch_reg_state {
	unsigned long watchlo[NUM_WATCH_REGS];
	u16 watchhi[NUM_WATCH_REGS];
};
union mips_watch_reg_state {
	struct mips3264_watch_reg_state mips3264;
};
#if defined(CONFIG_CPU_CAVIUM_OCTEON)
struct octeon_cop2_state {
	unsigned long	cop2_crc_iv;
	unsigned long	cop2_crc_length;
	unsigned long	cop2_crc_poly;
	unsigned long	cop2_llm_dat[2];
	unsigned long	cop2_3des_iv;
	unsigned long	cop2_3des_key[3];
	unsigned long	cop2_3des_result;
	unsigned long	cop2_aes_inp0;
	unsigned long	cop2_aes_iv[2];
	unsigned long	cop2_aes_key[4];
	unsigned long	cop2_aes_keylen;
	unsigned long	cop2_aes_result[2];
	unsigned long	cop2_hsh_datw[15];
	unsigned long	cop2_hsh_ivw[8];
	unsigned long	cop2_gfm_mult[2];
	unsigned long	cop2_gfm_poly;
	unsigned long	cop2_gfm_result[2];
	unsigned long	cop2_sha3[2];
};
#define COP2_INIT						\
	.cp2			= {0,},
#if defined(CONFIG_CAVIUM_OCTEON_CVMSEG_SIZE) && \
	CONFIG_CAVIUM_OCTEON_CVMSEG_SIZE > 0
struct octeon_cvmseg_state {
	unsigned long cvmseg[CONFIG_CAVIUM_OCTEON_CVMSEG_SIZE]
			    [cpu_dcache_line_size() / sizeof(unsigned long)];
};
#endif
#else
#define COP2_INIT
#endif
#ifdef CONFIG_CPU_HAS_MSA
# define ARCH_MIN_TASKALIGN	16
# define FPU_ALIGN		__aligned(16)
#else
# define ARCH_MIN_TASKALIGN	8
# define FPU_ALIGN
#endif
struct mips_abi;
struct thread_struct {
	unsigned long reg16;
	unsigned long reg17, reg18, reg19, reg20, reg21, reg22, reg23;
	unsigned long reg29, reg30, reg31;
	unsigned long cp0_status;
#ifdef CONFIG_MIPS_FP_SUPPORT
	struct mips_fpu_struct fpu FPU_ALIGN;
	atomic_t bd_emu_frame;
	unsigned long bd_emu_branch_pc;
	unsigned long bd_emu_cont_pc;
#endif
#ifdef CONFIG_MIPS_MT_FPAFF
	unsigned long emulated_fp;
	cpumask_t user_cpus_allowed;
#endif  
	struct mips_dsp_state dsp;
	union mips_watch_reg_state watch;
	unsigned long cp0_badvaddr;	 
	unsigned long cp0_baduaddr;	 
	unsigned long error_code;
	unsigned long trap_nr;
#ifdef CONFIG_CPU_CAVIUM_OCTEON
	struct octeon_cop2_state cp2 __attribute__ ((__aligned__(128)));
#if defined(CONFIG_CAVIUM_OCTEON_CVMSEG_SIZE) && \
	CONFIG_CAVIUM_OCTEON_CVMSEG_SIZE > 0
	struct octeon_cvmseg_state cvmseg __attribute__ ((__aligned__(128)));
#endif
#endif
	struct mips_abi *abi;
};
#ifdef CONFIG_MIPS_MT_FPAFF
#define FPAFF_INIT						\
	.emulated_fp			= 0,			\
	.user_cpus_allowed		= INIT_CPUMASK,
#else
#define FPAFF_INIT
#endif  
#ifdef CONFIG_MIPS_FP_SUPPORT
# define FPU_INIT						\
	.fpu			= {				\
		.fpr		= {{{0,},},},			\
		.fcr31		= 0,				\
		.msacsr		= 0,				\
	},							\
	 				\
	.bd_emu_frame = ATOMIC_INIT(BD_EMUFRAME_NONE),		\
	.bd_emu_branch_pc = 0,					\
	.bd_emu_cont_pc = 0,
#else
# define FPU_INIT
#endif
#define INIT_THREAD  {						\
	 							\
	.reg16			= 0,				\
	.reg17			= 0,				\
	.reg18			= 0,				\
	.reg19			= 0,				\
	.reg20			= 0,				\
	.reg21			= 0,				\
	.reg22			= 0,				\
	.reg23			= 0,				\
	.reg29			= 0,				\
	.reg30			= 0,				\
	.reg31			= 0,				\
	 							\
	.cp0_status		= 0,				\
	 							\
	FPU_INIT						\
	 							\
	FPAFF_INIT						\
	 							\
	.dsp			= {				\
		.dspr		= {0, },			\
		.dspcontrol	= 0,				\
	},							\
	 							\
	.watch = {{{0,},},},					\
	 							\
	.cp0_badvaddr		= 0,				\
	.cp0_baduaddr		= 0,				\
	.error_code		= 0,				\
	.trap_nr		= 0,				\
	 							\
	COP2_INIT						\
}
struct task_struct;
extern void start_thread(struct pt_regs * regs, unsigned long pc, unsigned long sp);
static inline void flush_thread(void)
{
}
unsigned long __get_wchan(struct task_struct *p);
#define __KSTK_TOS(tsk) ((unsigned long)task_stack_page(tsk) + \
			 THREAD_SIZE - 32 - sizeof(struct pt_regs))
#define task_pt_regs(tsk) ((struct pt_regs *)__KSTK_TOS(tsk))
#define KSTK_EIP(tsk) (task_pt_regs(tsk)->cp0_epc)
#define KSTK_ESP(tsk) (task_pt_regs(tsk)->regs[29])
#define KSTK_STATUS(tsk) (task_pt_regs(tsk)->cp0_status)
#define return_address() ({__asm__ __volatile__("":::"$31");__builtin_return_address(0);})
#ifdef CONFIG_CPU_HAS_PREFETCH
#define ARCH_HAS_PREFETCH
#define prefetch(x) __builtin_prefetch((x), 0, 1)
#define ARCH_HAS_PREFETCHW
#define prefetchw(x) __builtin_prefetch((x), 1, 1)
#endif
extern int mips_get_process_fp_mode(struct task_struct *task);
extern int mips_set_process_fp_mode(struct task_struct *task,
				    unsigned int value);
#define GET_FP_MODE(task)		mips_get_process_fp_mode(task)
#define SET_FP_MODE(task,value)		mips_set_process_fp_mode(task, value)
#endif  
