#ifndef __MIPS_UAPI_ASM_UCONTEXT_H
#define __MIPS_UAPI_ASM_UCONTEXT_H
struct extcontext {
	unsigned int		magic;
	unsigned int		size;
};
struct msa_extcontext {
	struct extcontext	ext;
#define MSA_EXTCONTEXT_MAGIC	0x784d5341	 
	unsigned long long	wr[32];
	unsigned int		csr;
};
#define END_EXTCONTEXT_MAGIC	0x78454e44	 
struct ucontext {
	unsigned long		uc_flags;
	struct ucontext		*uc_link;
	stack_t			uc_stack;
	struct sigcontext	uc_mcontext;
	sigset_t		uc_sigmask;
	unsigned long long	uc_extcontext[];
};
#endif  
