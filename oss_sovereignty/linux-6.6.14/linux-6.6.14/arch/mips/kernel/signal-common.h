#ifndef __SIGNAL_COMMON_H
#define __SIGNAL_COMMON_H
#ifdef DEBUG_SIG
#  define DEBUGP(fmt, args...) printk("%s: " fmt, __func__, ##args)
#else
#  define DEBUGP(fmt, args...)
#endif
extern void __user *get_sigframe(struct ksignal *ksig, struct pt_regs *regs,
				 size_t frame_size);
extern int fpcsr_pending(unsigned int __user *fpcsr);
#define lock_fpu_owner()	({ preempt_disable(); pagefault_disable(); })
#define unlock_fpu_owner()	({ pagefault_enable(); preempt_enable(); })
extern asmlinkage int
_save_fp_context(void __user *fpregs, void __user *csr);
extern asmlinkage int
_restore_fp_context(void __user *fpregs, void __user *csr);
extern asmlinkage int _save_msa_all_upper(void __user *buf);
extern asmlinkage int _restore_msa_all_upper(void __user *buf);
#endif	 
