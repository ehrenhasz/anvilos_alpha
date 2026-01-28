#ifndef _ASM_POWERPC_PACA_H
#define _ASM_POWERPC_PACA_H
#ifdef __KERNEL__
#ifdef CONFIG_PPC64
#include <linux/cache.h>
#include <linux/string.h>
#include <asm/types.h>
#include <asm/mmu.h>
#include <asm/page.h>
#ifdef CONFIG_PPC_BOOK3E_64
#include <asm/exception-64e.h>
#else
#include <asm/exception-64s.h>
#endif
#ifdef CONFIG_KVM_BOOK3S_64_HANDLER
#include <asm/kvm_book3s_asm.h>
#endif
#include <asm/accounting.h>
#include <asm/hmi.h>
#include <asm/cpuidle.h>
#include <asm/atomic.h>
#include <asm/mce.h>
#include <asm-generic/mmiowb_types.h>
register struct paca_struct *local_paca asm("r13");
#if defined(CONFIG_DEBUG_PREEMPT) && defined(CONFIG_SMP)
extern unsigned int debug_smp_processor_id(void);  
#define get_paca()	((void) debug_smp_processor_id(), local_paca)
#else
#define get_paca()	local_paca
#endif
#define get_slb_shadow()	(get_paca()->slb_shadow_ptr)
struct task_struct;
struct rtas_args;
struct lppaca;
struct paca_struct {
#ifdef CONFIG_PPC_PSERIES
	struct lppaca *lppaca_ptr;	 
#endif  
#ifdef __BIG_ENDIAN__
	u16 lock_token;			 
	u16 paca_index;			 
#else
	u16 paca_index;			 
	u16 lock_token;			 
#endif
#ifndef CONFIG_PPC_KERNEL_PCREL
	u64 kernel_toc;			 
#endif
	u64 kernelbase;			 
	u64 kernel_msr;			 
	void *emergency_sp;		 
	u64 data_offset;		 
	s16 hw_cpu_id;			 
	u8 cpu_start;			 
	u8 kexec_state;		 
#ifdef CONFIG_PPC_BOOK3S_64
#ifdef CONFIG_PPC_64S_HASH_MMU
	struct slb_shadow *slb_shadow_ptr;
#endif
	struct dtl_entry *dispatch_log;
	struct dtl_entry *dispatch_log_end;
#endif
	u64 dscr_default;		 
#ifdef CONFIG_PPC_BOOK3S_64
	u64 exgen[EX_SIZE] __attribute__((aligned(0x80)));
#ifdef CONFIG_PPC_64S_HASH_MMU
	u16 vmalloc_sllp;
	u8 slb_cache_ptr;
	u8 stab_rr;			 
#ifdef CONFIG_DEBUG_VM
	u8 in_kernel_slb_handler;
#endif
	u32 slb_used_bitmap;		 
	u32 slb_kern_bitmap;
	u32 slb_cache[SLB_CACHE_ENTRIES];
#endif
#endif  
#ifdef CONFIG_PPC_BOOK3E_64
	u64 exgen[8] __aligned(0x40);
	pgd_t *pgd __aligned(0x40);  
	pgd_t *kernel_pgd;		 
	struct tlb_core_data *tcd_ptr;
	u64 extlb[12][EX_TLB_SIZE / sizeof(u64)];
	u64 exmc[8];		 
	u64 excrit[8];		 
	u64 exdbg[8];		 
	void *mc_kstack;
	void *crit_kstack;
	void *dbg_kstack;
	struct tlb_core_data tcd;
#endif  
#ifdef CONFIG_PPC_64S_HASH_MMU
	unsigned char mm_ctx_low_slices_psize[BITS_PER_LONG / BITS_PER_BYTE];
	unsigned char mm_ctx_high_slices_psize[SLICE_ARRAY_SIZE];
#endif
	struct task_struct *__current;	 
	u64 kstack;			 
	u64 saved_r1;			 
	u64 saved_msr;			 
#ifdef CONFIG_PPC64
	u64 exit_save_r1;		 
#endif
#ifdef CONFIG_PPC_BOOK3E_64
	u16 trap_save;			 
#endif
#ifdef CONFIG_PPC_BOOK3S_64
	u8 hsrr_valid;			 
	u8 srr_valid;			 
#endif
	u8 irq_soft_mask;		 
	u8 irq_happened;		 
	u8 irq_work_pending;		 
#ifdef CONFIG_KVM_BOOK3S_HV_POSSIBLE
	u8 pmcregs_in_use;		 
#endif
	u64 sprg_vdso;			 
#ifdef CONFIG_PPC_TRANSACTIONAL_MEM
	u64 tm_scratch;                  
#endif
#ifdef CONFIG_PPC_POWERNV
	unsigned long idle_lock;  
	unsigned long idle_state;
	union {
		struct {
			u8 thread_idle_state;
			u8 subcore_sibling_mask;
		};
		struct {
#ifdef CONFIG_KVM_BOOK3S_HV_POSSIBLE
			u64 requested_psscr;
			atomic_t dont_stop;
#endif
		};
	};
#endif
#ifdef CONFIG_PPC_BOOK3S_64
	u64 exnmi[EX_SIZE];	 
	u64 exmc[EX_SIZE];	 
#endif
#ifdef CONFIG_PPC_BOOK3S_64
	void *nmi_emergency_sp;
	void *mc_emergency_sp;
	u16 in_nmi;			 
	u16 in_mce;
	u8 hmi_event_available;		 
	u8 hmi_p9_special_emu;		 
	u32 hmi_irqs;			 
#endif
	u8 ftrace_enabled;		 
	struct cpu_accounting_data accounting;
	u64 dtl_ridx;			 
	struct dtl_entry *dtl_curr;	 
#ifdef CONFIG_KVM_BOOK3S_HANDLER
#ifdef CONFIG_KVM_BOOK3S_PR_POSSIBLE
	struct kvmppc_book3s_shadow_vcpu shadow_vcpu;
#endif
	struct kvmppc_host_state kvm_hstate;
#ifdef CONFIG_KVM_BOOK3S_HV_POSSIBLE
	struct sibling_subcore_state *sibling_subcore_state;
#endif
#endif
#ifdef CONFIG_PPC_BOOK3S_64
	u64 exrfi[EX_SIZE] __aligned(0x80);
	void *rfi_flush_fallback_area;
	u64 l1d_flush_size;
#endif
#ifdef CONFIG_PPC_PSERIES
	u8 *mce_data_buf;		 
#endif  
#ifdef CONFIG_PPC_BOOK3S_64
#ifdef CONFIG_PPC_64S_HASH_MMU
	struct slb_entry *mce_faulty_slbs;
	u16 slb_save_cache_ptr;
#endif
#endif  
#ifdef CONFIG_STACKPROTECTOR
	unsigned long canary;
#endif
#ifdef CONFIG_MMIOWB
	struct mmiowb_state mmiowb_state;
#endif
#ifdef CONFIG_PPC_BOOK3S_64
	struct mce_info *mce_info;
	u8 mce_pending_irq_work;
#endif  
} ____cacheline_aligned;
extern void copy_mm_to_paca(struct mm_struct *mm);
extern struct paca_struct **paca_ptrs;
extern void initialise_paca(struct paca_struct *new_paca, int cpu);
extern void setup_paca(struct paca_struct *new_paca);
extern void allocate_paca_ptrs(void);
extern void allocate_paca(int cpu);
extern void free_unused_pacas(void);
#else  
static inline void allocate_paca(int cpu) { }
static inline void free_unused_pacas(void) { }
#endif  
#endif  
#endif  
