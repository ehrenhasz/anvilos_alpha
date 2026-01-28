#ifndef __ASM_SH_USER_H
#define __ASM_SH_USER_H
#include <asm/ptrace.h>
#include <asm/page.h>
struct user_fpu_struct {
	unsigned long fp_regs[16];
	unsigned long xfp_regs[16];
	unsigned long fpscr;
	unsigned long fpul;
};
struct user {
	struct pt_regs	regs;			 
	struct user_fpu_struct fpu;	 
	int u_fpvalid;		 
	size_t		u_tsize;		 
	size_t		u_dsize;		 
	size_t		u_ssize;		 
	unsigned long	start_code;		 
	unsigned long	start_data;		 
	unsigned long	start_stack;		 
	long int	signal;			 
	unsigned long	u_ar0;			 
	struct user_fpu_struct* u_fpstate;	 
	unsigned long	magic;			 
	char		u_comm[32];		 
};
#endif  
