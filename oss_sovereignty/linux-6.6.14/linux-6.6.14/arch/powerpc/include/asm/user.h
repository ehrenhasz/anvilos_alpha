#ifndef _ASM_POWERPC_USER_H
#define _ASM_POWERPC_USER_H
#include <asm/ptrace.h>
#include <asm/page.h>
struct user {
	struct user_pt_regs regs;		 
	size_t		u_tsize;		 
	size_t		u_dsize;		 
	size_t		u_ssize;		 
	unsigned long	start_code;		 
	unsigned long	start_data;		 
	unsigned long	start_stack;		 
	long int	signal;			 
	unsigned long	u_ar0;			 
	unsigned long	magic;			 
	char		u_comm[32];		 
};
#endif	 
