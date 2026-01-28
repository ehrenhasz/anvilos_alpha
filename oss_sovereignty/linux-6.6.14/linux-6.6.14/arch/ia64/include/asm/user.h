#ifndef _ASM_IA64_USER_H
#define _ASM_IA64_USER_H
#include <linux/ptrace.h>
#include <linux/types.h>
#include <asm/page.h>
#define EF_SIZE		3072	 
struct user {
	unsigned long	regs[EF_SIZE/8+32];	 
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
