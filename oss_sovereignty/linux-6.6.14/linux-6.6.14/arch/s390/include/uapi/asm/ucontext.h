#ifndef _ASM_S390_UCONTEXT_H
#define _ASM_S390_UCONTEXT_H
#define UC_GPRS_HIGH	1	 
#define UC_VXRS		2	 
struct ucontext_extended {
	unsigned long	  uc_flags;
	struct ucontext  *uc_link;
	stack_t		  uc_stack;
	_sigregs	  uc_mcontext;
	sigset_t	  uc_sigmask;
	unsigned char	  __unused[128 - sizeof(sigset_t)];
	_sigregs_ext	  uc_mcontext_ext;
};
struct ucontext {
	unsigned long	  uc_flags;
	struct ucontext  *uc_link;
	stack_t		  uc_stack;
	_sigregs          uc_mcontext;
	sigset_t	  uc_sigmask;
	unsigned char	  __unused[128 - sizeof(sigset_t)];
};
#endif  
