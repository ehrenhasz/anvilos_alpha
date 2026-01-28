#ifndef _S390_USER_H
#define _S390_USER_H
#include <asm/page.h>
#include <asm/ptrace.h>
struct user {
  struct user_regs_struct regs;		 
  unsigned long int u_tsize;	 
  unsigned long int u_dsize;	 
  unsigned long int u_ssize;	 
  unsigned long start_code;      
  unsigned long start_stack;	 
  long int signal;     		 
  unsigned long u_ar0;		 
  unsigned long magic;		 
  char u_comm[32];		 
};
#endif  
