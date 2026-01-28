#ifndef _ENTRY_H
#define _ENTRY_H
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/init.h>
void handler_irq(int irq, struct pt_regs *regs);
#ifdef CONFIG_SPARC32
void do_hw_interrupt(struct pt_regs *regs, unsigned long type);
void do_illegal_instruction(struct pt_regs *regs, unsigned long pc,
                            unsigned long npc, unsigned long psr);
void do_priv_instruction(struct pt_regs *regs, unsigned long pc,
                         unsigned long npc, unsigned long psr);
void do_memaccess_unaligned(struct pt_regs *regs, unsigned long pc,
                            unsigned long npc, unsigned long psr);
void do_fpd_trap(struct pt_regs *regs, unsigned long pc,
                 unsigned long npc, unsigned long psr);
void do_fpe_trap(struct pt_regs *regs, unsigned long pc,
                 unsigned long npc, unsigned long psr);
void handle_tag_overflow(struct pt_regs *regs, unsigned long pc,
                         unsigned long npc, unsigned long psr);
void handle_watchpoint(struct pt_regs *regs, unsigned long pc,
                       unsigned long npc, unsigned long psr);
void handle_reg_access(struct pt_regs *regs, unsigned long pc,
                       unsigned long npc, unsigned long psr);
void handle_cp_disabled(struct pt_regs *regs, unsigned long pc,
                        unsigned long npc, unsigned long psr);
void handle_cp_exception(struct pt_regs *regs, unsigned long pc,
                         unsigned long npc, unsigned long psr);
void fpsave(unsigned long *fpregs, unsigned long *fsr,
            void *fpqueue, unsigned long *fpqdepth);
void fpload(unsigned long *fpregs, unsigned long *fsr);
#else  
#include <asm/trap_block.h>
struct popc_3insn_patch_entry {
	unsigned int	addr;
	unsigned int	insns[3];
};
extern struct popc_3insn_patch_entry __popc_3insn_patch,
	__popc_3insn_patch_end;
struct popc_6insn_patch_entry {
	unsigned int	addr;
	unsigned int	insns[6];
};
extern struct popc_6insn_patch_entry __popc_6insn_patch,
	__popc_6insn_patch_end;
struct pause_patch_entry {
	unsigned int	addr;
	unsigned int	insns[3];
};
extern struct pause_patch_entry __pause_3insn_patch,
	__pause_3insn_patch_end;
void sun4v_patch_1insn_range(struct sun4v_1insn_patch_entry *,
			     struct sun4v_1insn_patch_entry *);
void sun4v_patch_2insn_range(struct sun4v_2insn_patch_entry *,
			     struct sun4v_2insn_patch_entry *);
void sun_m7_patch_2insn_range(struct sun4v_2insn_patch_entry *,
			     struct sun4v_2insn_patch_entry *);
extern unsigned int dcache_parity_tl1_occurred;
extern unsigned int icache_parity_tl1_occurred;
asmlinkage void sparc_breakpoint(struct pt_regs *regs);
void timer_interrupt(int irq, struct pt_regs *regs);
void do_notify_resume(struct pt_regs *regs,
		      unsigned long orig_i0,
		      unsigned long thread_info_flags);
asmlinkage int syscall_trace_enter(struct pt_regs *regs);
asmlinkage void syscall_trace_leave(struct pt_regs *regs);
void bad_trap_tl1(struct pt_regs *regs, long lvl);
void do_fpieee(struct pt_regs *regs);
void do_fpother(struct pt_regs *regs);
void do_tof(struct pt_regs *regs);
void do_div0(struct pt_regs *regs);
void do_illegal_instruction(struct pt_regs *regs);
void mem_address_unaligned(struct pt_regs *regs,
			   unsigned long sfar,
			   unsigned long sfsr);
void sun4v_do_mna(struct pt_regs *regs,
		  unsigned long addr,
		  unsigned long type_ctx);
void do_privop(struct pt_regs *regs);
void do_privact(struct pt_regs *regs);
void do_cee(struct pt_regs *regs);
void do_div0_tl1(struct pt_regs *regs);
void do_fpieee_tl1(struct pt_regs *regs);
void do_fpother_tl1(struct pt_regs *regs);
void do_ill_tl1(struct pt_regs *regs);
void do_irq_tl1(struct pt_regs *regs);
void do_lddfmna_tl1(struct pt_regs *regs);
void do_stdfmna_tl1(struct pt_regs *regs);
void do_paw(struct pt_regs *regs);
void do_paw_tl1(struct pt_regs *regs);
void do_vaw(struct pt_regs *regs);
void do_vaw_tl1(struct pt_regs *regs);
void do_tof_tl1(struct pt_regs *regs);
void do_getpsr(struct pt_regs *regs);
void spitfire_insn_access_exception(struct pt_regs *regs,
				    unsigned long sfsr,
				    unsigned long sfar);
void spitfire_insn_access_exception_tl1(struct pt_regs *regs,
				        unsigned long sfsr,
				        unsigned long sfar);
void spitfire_data_access_exception(struct pt_regs *regs,
				    unsigned long sfsr,
				    unsigned long sfar);
void spitfire_data_access_exception_tl1(struct pt_regs *regs,
				        unsigned long sfsr,
				        unsigned long sfar);
void spitfire_access_error(struct pt_regs *regs,
			   unsigned long status_encoded,
			   unsigned long afar);
void cheetah_fecc_handler(struct pt_regs *regs,
			  unsigned long afsr,
			  unsigned long afar);
void cheetah_cee_handler(struct pt_regs *regs,
			 unsigned long afsr,
			 unsigned long afar);
void cheetah_deferred_handler(struct pt_regs *regs,
			      unsigned long afsr,
			      unsigned long afar);
void cheetah_plus_parity_error(int type, struct pt_regs *regs);
void sun4v_insn_access_exception(struct pt_regs *regs,
				 unsigned long addr,
				 unsigned long type_ctx);
void sun4v_insn_access_exception_tl1(struct pt_regs *regs,
				     unsigned long addr,
				     unsigned long type_ctx);
void sun4v_data_access_exception(struct pt_regs *regs,
				 unsigned long addr,
				 unsigned long type_ctx);
void sun4v_data_access_exception_tl1(struct pt_regs *regs,
				     unsigned long addr,
				     unsigned long type_ctx);
void sun4v_resum_error(struct pt_regs *regs,
		       unsigned long offset);
void sun4v_resum_overflow(struct pt_regs *regs);
void sun4v_nonresum_error(struct pt_regs *regs,
			  unsigned long offset);
void sun4v_nonresum_overflow(struct pt_regs *regs);
void sun4v_mem_corrupt_detect_precise(struct pt_regs *regs,
				      unsigned long addr,
				      unsigned long context);
extern unsigned long sun4v_err_itlb_vaddr;
extern unsigned long sun4v_err_itlb_ctx;
extern unsigned long sun4v_err_itlb_pte;
extern unsigned long sun4v_err_itlb_error;
void sun4v_itlb_error_report(struct pt_regs *regs, int tl);
extern unsigned long sun4v_err_dtlb_vaddr;
extern unsigned long sun4v_err_dtlb_ctx;
extern unsigned long sun4v_err_dtlb_pte;
extern unsigned long sun4v_err_dtlb_error;
void sun4v_dtlb_error_report(struct pt_regs *regs, int tl);
void hypervisor_tlbop_error(unsigned long err,
			    unsigned long op);
void hypervisor_tlbop_error_xcall(unsigned long err,
				  unsigned long op);
struct cheetah_err_info {
 u64 afsr;
 u64 afar;
 u64 dcache_data[4];	 
 u64 dcache_index;	 
 u64 dcache_tag;		 
 u64 dcache_utag;	 
 u64 dcache_stag;	 
 u64 icache_data[8];	 
 u64 icache_index;	 
 u64 icache_tag;		 
 u64 icache_utag;	 
 u64 icache_stag;	 
 u64 icache_upper;	 
 u64 icache_lower;	 
 u64 ecache_data[4];	 
 u64 ecache_index;	 
 u64 ecache_tag;		 
 u64 __pad[32 - 30];
};
#define CHAFSR_INVALID		((u64)-1L)
extern struct cheetah_err_info *cheetah_error_log;
struct ino_bucket {
 unsigned long __irq_chain_pa;
 unsigned int __irq;
 unsigned int __pad;
};
extern struct ino_bucket *ivector_table;
extern unsigned long ivector_table_pa;
void init_irqwork_curcpu(void);
void sun4v_register_mondo_queues(int this_cpu);
#endif  
#endif  
