#ifndef _ASM_POWERPC_PROCESSOR_H
#define _ASM_POWERPC_PROCESSOR_H
#include <vdso/processor.h>
#include <asm/reg.h>
#ifdef CONFIG_VSX
#define TS_FPRWIDTH 2
#ifdef __BIG_ENDIAN__
#define TS_FPROFFSET 0
#define TS_VSRLOWOFFSET 1
#else
#define TS_FPROFFSET 1
#define TS_VSRLOWOFFSET 0
#endif
#else
#define TS_FPRWIDTH 1
#define TS_FPROFFSET 0
#endif
#ifdef CONFIG_PPC64
#define PPR_PRIORITY 3
#ifdef __ASSEMBLY__
#define DEFAULT_PPR (PPR_PRIORITY << 50)
#else
#define DEFAULT_PPR ((u64)PPR_PRIORITY << 50)
#endif  
#endif  
#ifndef __ASSEMBLY__
#include <linux/types.h>
#include <linux/thread_info.h>
#include <asm/ptrace.h>
#include <asm/hw_breakpoint.h>
#define _PREP_Motorola	0x01	 
#define _PREP_Firm	0x02	 
#define _PREP_IBM	0x00	 
#define _PREP_Bull	0x03	 
#define _CHRP_Motorola	0x04	 
#define _CHRP_IBM	0x05	 
#define _CHRP_Pegasos	0x06	 
#define _CHRP_briq	0x07	 
#if defined(__KERNEL__) && defined(CONFIG_PPC32)
extern int _chrp_type;
#endif  
#ifdef __KERNEL__
#ifdef CONFIG_PPC64
#include <asm/task_size_64.h>
#else
#include <asm/task_size_32.h>
#endif
struct task_struct;
void start_thread(struct pt_regs *regs, unsigned long fdptr, unsigned long sp);
#define TS_FPR(i) fp_state.fpr[i][TS_FPROFFSET]
#define TS_CKFPR(i) ckfp_state.fpr[i][TS_FPROFFSET]
struct thread_fp_state {
	u64	fpr[32][TS_FPRWIDTH] __attribute__((aligned(16)));
	u64	fpscr;		 
};
struct thread_vr_state {
	vector128	vr[32] __attribute__((aligned(16)));
	vector128	vscr __attribute__((aligned(16)));
};
struct debug_reg {
#ifdef CONFIG_PPC_ADV_DEBUG_REGS
	uint32_t	dbcr0;
	uint32_t	dbcr1;
#ifdef CONFIG_BOOKE
	uint32_t	dbcr2;
#endif
	uint32_t	dbsr;
	unsigned long	iac1;
	unsigned long	iac2;
#if CONFIG_PPC_ADV_DEBUG_IACS > 2
	unsigned long	iac3;
	unsigned long	iac4;
#endif
	unsigned long	dac1;
	unsigned long	dac2;
#if CONFIG_PPC_ADV_DEBUG_DVCS > 0
	unsigned long	dvc1;
	unsigned long	dvc2;
#endif
#endif
};
struct thread_struct {
	unsigned long	ksp;		 
#ifdef CONFIG_PPC64
	unsigned long	ksp_vsid;
#endif
	struct pt_regs	*regs;		 
#ifdef CONFIG_BOOKE
	unsigned long	normsave[8] ____cacheline_aligned;
#endif
#ifdef CONFIG_PPC32
	void		*pgdir;		 
#ifdef CONFIG_PPC_RTAS
	unsigned long	rtas_sp;	 
#endif
#if defined(CONFIG_PPC_BOOK3S_32) && defined(CONFIG_PPC_KUAP)
	unsigned long	kuap;		 
#endif
	unsigned long	srr0;
	unsigned long	srr1;
	unsigned long	dar;
	unsigned long	dsisr;
#ifdef CONFIG_PPC_BOOK3S_32
	unsigned long	r0, r3, r4, r5, r6, r8, r9, r11;
	unsigned long	lr, ctr;
	unsigned long	sr0;
#endif
#endif  
#if defined(CONFIG_BOOKE_OR_40x) && defined(CONFIG_PPC_KUAP)
	unsigned long	pid;	 
#endif
	struct debug_reg debug;
#ifdef CONFIG_PPC_FPU_REGS
	struct thread_fp_state	fp_state;
	struct thread_fp_state	*fp_save_area;
#endif
	int		fpexc_mode;	 
	unsigned int	align_ctl;	 
#ifdef CONFIG_HAVE_HW_BREAKPOINT
	struct perf_event *ptrace_bps[HBP_NUM_MAX];
#endif  
	struct arch_hw_breakpoint hw_brk[HBP_NUM_MAX];  
	unsigned long	trap_nr;	 
	u8 load_slb;			 
	u8 load_fp;
#ifdef CONFIG_ALTIVEC
	u8 load_vec;
	struct thread_vr_state vr_state;
	struct thread_vr_state *vr_save_area;
	unsigned long	vrsave;
	int		used_vr;	 
#endif  
#ifdef CONFIG_VSX
	int		used_vsr;	 
#endif  
#ifdef CONFIG_SPE
	struct_group(spe,
		unsigned long	evr[32];	 
		u64		acc;		 
	);
	unsigned long	spefscr;	 
	unsigned long	spefscr_last;	 
	int		used_spe;	 
#endif  
#ifdef CONFIG_PPC_TRANSACTIONAL_MEM
	u8	load_tm;
	u64		tm_tfhar;	 
	u64		tm_texasr;	 
	u64		tm_tfiar;	 
	struct pt_regs	ckpt_regs;	 
	unsigned long	tm_tar;
	unsigned long	tm_ppr;
	unsigned long	tm_dscr;
	unsigned long   tm_amr;
	struct thread_fp_state ckfp_state;  
	struct thread_vr_state ckvr_state;  
	unsigned long	ckvrsave;  
#endif  
#ifdef CONFIG_KVM_BOOK3S_32_HANDLER
	void*		kvm_shadow_vcpu;  
#endif  
#if defined(CONFIG_KVM) && defined(CONFIG_BOOKE)
	struct kvm_vcpu	*kvm_vcpu;
#endif
#ifdef CONFIG_PPC64
	unsigned long	dscr;
	unsigned long	fscr;
	int		dscr_inherit;
	unsigned long	tidr;
#endif
#ifdef CONFIG_PPC_BOOK3S_64
	unsigned long	tar;
	unsigned long	ebbrr;
	unsigned long	ebbhr;
	unsigned long	bescr;
	unsigned long	siar;
	unsigned long	sdar;
	unsigned long	sier;
	unsigned long	mmcr2;
	unsigned 	mmcr0;
	unsigned 	used_ebb;
	unsigned long   mmcr3;
	unsigned long   sier2;
	unsigned long   sier3;
	unsigned long	hashkeyr;
#endif
};
#define ARCH_MIN_TASKALIGN 16
#define INIT_SP		(sizeof(init_stack) + (unsigned long) &init_stack)
#define INIT_SP_LIMIT	((unsigned long)&init_stack)
#ifdef CONFIG_SPE
#define SPEFSCR_INIT \
	.spefscr = SPEFSCR_FINVE | SPEFSCR_FDBZE | SPEFSCR_FUNFE | SPEFSCR_FOVFE, \
	.spefscr_last = SPEFSCR_FINVE | SPEFSCR_FDBZE | SPEFSCR_FUNFE | SPEFSCR_FOVFE,
#else
#define SPEFSCR_INIT
#endif
#ifdef CONFIG_PPC_BOOK3S_32
#define SR0_INIT	.sr0 = IS_ENABLED(CONFIG_PPC_KUEP) ? SR_NX : 0,
#else
#define SR0_INIT
#endif
#if defined(CONFIG_PPC_BOOK3S_32) && defined(CONFIG_PPC_KUAP)
#define INIT_THREAD { \
	.ksp = INIT_SP, \
	.pgdir = swapper_pg_dir, \
	.kuap = ~0UL,   \
	.fpexc_mode = MSR_FE0 | MSR_FE1, \
	SPEFSCR_INIT \
	SR0_INIT \
}
#elif defined(CONFIG_PPC32)
#define INIT_THREAD { \
	.ksp = INIT_SP, \
	.pgdir = swapper_pg_dir, \
	.fpexc_mode = MSR_FE0 | MSR_FE1, \
	SPEFSCR_INIT \
	SR0_INIT \
}
#else
#define INIT_THREAD  { \
	.ksp = INIT_SP, \
	.fpexc_mode = 0, \
}
#endif
#define task_pt_regs(tsk)	((tsk)->thread.regs)
unsigned long __get_wchan(struct task_struct *p);
#define KSTK_EIP(tsk)  ((tsk)->thread.regs? (tsk)->thread.regs->nip: 0)
#define KSTK_ESP(tsk)  ((tsk)->thread.regs? (tsk)->thread.regs->gpr[1]: 0)
#define GET_FPEXC_CTL(tsk, adr) get_fpexc_mode((tsk), (adr))
#define SET_FPEXC_CTL(tsk, val) set_fpexc_mode((tsk), (val))
extern int get_fpexc_mode(struct task_struct *tsk, unsigned long adr);
extern int set_fpexc_mode(struct task_struct *tsk, unsigned int val);
#define GET_ENDIAN(tsk, adr) get_endian((tsk), (adr))
#define SET_ENDIAN(tsk, val) set_endian((tsk), (val))
extern int get_endian(struct task_struct *tsk, unsigned long adr);
extern int set_endian(struct task_struct *tsk, unsigned int val);
#define GET_UNALIGN_CTL(tsk, adr)	get_unalign_ctl((tsk), (adr))
#define SET_UNALIGN_CTL(tsk, val)	set_unalign_ctl((tsk), (val))
extern int get_unalign_ctl(struct task_struct *tsk, unsigned long adr);
extern int set_unalign_ctl(struct task_struct *tsk, unsigned int val);
extern void load_fp_state(struct thread_fp_state *fp);
extern void store_fp_state(struct thread_fp_state *fp);
extern void load_vr_state(struct thread_vr_state *vr);
extern void store_vr_state(struct thread_vr_state *vr);
static inline unsigned int __unpack_fe01(unsigned long msr_bits)
{
	return ((msr_bits & MSR_FE0) >> 10) | ((msr_bits & MSR_FE1) >> 8);
}
static inline unsigned long __pack_fe01(unsigned int fpmode)
{
	return ((fpmode << 10) & MSR_FE0) | ((fpmode << 8) & MSR_FE1);
}
#ifdef CONFIG_PPC64
#define spin_begin()							\
	asm volatile(ASM_FTR_IFCLR(					\
		"or 1,1,1",  				\
		"nop",  	\
		%0) :: "i" (CPU_FTR_ARCH_31) : "memory")
#define spin_cpu_relax()						\
	asm volatile(ASM_FTR_IFCLR(					\
		"nop",   \
		PPC_WAIT(2, 0),	 			\
		%0) :: "i" (CPU_FTR_ARCH_31) : "memory")
#define spin_end()							\
	asm volatile(ASM_FTR_IFCLR(					\
		"or 2,2,2",  				\
		"nop",							\
		%0) :: "i" (CPU_FTR_ARCH_31) : "memory")
#endif
int validate_sp(unsigned long sp, struct task_struct *p);
int validate_sp_size(unsigned long sp, struct task_struct *p,
		     unsigned long nbytes);
#define ARCH_HAS_PREFETCH
#define ARCH_HAS_PREFETCHW
static inline void prefetch(const void *x)
{
	if (unlikely(!x))
		return;
	__asm__ __volatile__ ("dcbt 0,%0" : : "r" (x));
}
static inline void prefetchw(const void *x)
{
	if (unlikely(!x))
		return;
	__asm__ __volatile__ ("dcbtst 0,%0" : : "r" (x));
}
extern unsigned long isa300_idle_stop_noloss(unsigned long psscr_val);
extern unsigned long isa300_idle_stop_mayloss(unsigned long psscr_val);
extern unsigned long isa206_idle_insn_mayloss(unsigned long type);
#ifdef CONFIG_PPC_970_NAP
extern void power4_idle_nap(void);
void power4_idle_nap_return(void);
#endif
extern unsigned long cpuidle_disable;
enum idle_boot_override {IDLE_NO_OVERRIDE = 0, IDLE_POWERSAVE_OFF};
extern int powersave_nap;	 
extern void power7_idle_type(unsigned long type);
extern void arch300_idle_type(unsigned long stop_psscr_val,
			      unsigned long stop_psscr_mask);
void pnv_power9_force_smt4_catch(void);
void pnv_power9_force_smt4_release(void);
extern int fix_alignment(struct pt_regs *);
#ifdef CONFIG_PPC64
#define NET_IP_ALIGN	0
#endif
int do_mathemu(struct pt_regs *regs);
int do_spe_mathemu(struct pt_regs *regs);
int speround_handler(struct pt_regs *regs);
int enter_vmx_usercopy(void);
int exit_vmx_usercopy(void);
int enter_vmx_ops(void);
void *exit_vmx_ops(void *dest);
#endif  
#endif  
#endif  
