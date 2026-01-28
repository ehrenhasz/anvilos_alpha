#ifndef _ALPHA_USER_H
#define _ALPHA_USER_H
#include <linux/sched.h>
#include <linux/ptrace.h>
#include <asm/page.h>
#include <asm/reg.h>
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
