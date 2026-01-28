#ifndef _ASM_POWERPC_LPPACA_H
#define _ASM_POWERPC_LPPACA_H
#ifdef __KERNEL__
#ifdef CONFIG_PPC_BOOK3S
#include <linux/cache.h>
#include <linux/threads.h>
#include <asm/types.h>
#include <asm/mmu.h>
#include <asm/firmware.h>
#include <asm/paca.h>
struct lppaca {
	__be32	desc;			 
	__be16	size;			 
	u8	reserved1[3];
	u8	__old_status;		 
	u8	reserved3[14];
	volatile __be32 dyn_hw_node_id;	 
	volatile __be32 dyn_hw_proc_id;	 
	u8	reserved4[56];
	volatile u8 vphn_assoc_counts[8];  
	u8	reserved5[32];
	u8	reserved6[48];
	u8	cede_latency_hint;
	u8	ebb_regs_in_use;
	u8	reserved7[6];
	u8	dtl_enable_mask;	 
	u8	donate_dedicated_cpu;	 
	u8	fpregs_in_use;
	u8	pmcregs_in_use;
	u8	reserved8[28];
	__be64	wait_state_cycles;	 
	u8	reserved9[28];
	__be16	slb_count;		 
	u8	idle;			 
	u8	vmxregs_in_use;
	volatile __be32 yield_count;
	volatile __be32 dispersion_count;  
	volatile __be64 cmo_faults;	 
	volatile __be64 cmo_fault_time;	 
	u8	reserved10[64];		 
	volatile __be64 enqueue_dispatch_tb;  
	volatile __be64 ready_enqueue_tb;  
	volatile __be64 wait_ready_tb;	 
	u8	reserved11[16];
	__be32	page_ins;		 
	u8	reserved12[148];
	volatile __be64 dtl_idx;	 
	u8	reserved13[96];
} ____cacheline_aligned;
#define lppaca_of(cpu)	(*paca_ptrs[cpu]->lppaca_ptr)
#define LPPACA_OLD_SHARED_PROC		2
#ifdef CONFIG_PPC_PSERIES
static inline bool lppaca_shared_proc(void)
{
	struct lppaca *l = local_paca->lppaca_ptr;
	if (!firmware_has_feature(FW_FEATURE_SPLPAR))
		return false;
	return !!(l->__old_status & LPPACA_OLD_SHARED_PROC);
}
#define get_lppaca()	(get_paca()->lppaca_ptr)
#endif
struct slb_shadow {
	__be32	persistent;		 
	__be32	buffer_length;		 
	__be64	reserved;
	struct	{
		__be64     esid;
		__be64	vsid;
	} save_area[SLB_NUM_BOLTED];
} ____cacheline_aligned;
#endif  
#endif  
#endif  
