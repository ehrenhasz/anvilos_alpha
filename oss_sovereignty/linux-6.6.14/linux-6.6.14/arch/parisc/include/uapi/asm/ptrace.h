#ifndef _UAPI_PARISC_PTRACE_H
#define _UAPI_PARISC_PTRACE_H
#include <linux/types.h>
struct pt_regs {
	unsigned long gr[32];	 
	__u64 fr[32];
	unsigned long sr[ 8];
	unsigned long iasq[2];
	unsigned long iaoq[2];
	unsigned long cr27;
	unsigned long pad0;      
	unsigned long orig_r28;
	unsigned long ksp;
	unsigned long kpc;
	unsigned long sar;	 
	unsigned long iir;	 
	unsigned long isr;	 
	unsigned long ior;	 
	unsigned long ipsw;	 
};
struct user_regs_struct {
	unsigned long gr[32];	 
	unsigned long sr[8];
	unsigned long iaoq[2];
	unsigned long iasq[2];
	unsigned long sar;	 
	unsigned long iir;	 
	unsigned long isr;	 
	unsigned long ior;	 
	unsigned long ipsw;	 
	unsigned long cr0;
	unsigned long cr24, cr25, cr26, cr27, cr28, cr29, cr30, cr31;
	unsigned long cr8, cr9, cr12, cr13, cr10, cr15;
	unsigned long _pad[80-64];	 
};
struct user_fp_struct {
	__u64 fr[32];
};
#define PTRACE_SINGLEBLOCK	12	 
#define PTRACE_GETREGS		18
#define PTRACE_SETREGS		19
#define PTRACE_GETFPREGS	14
#define PTRACE_SETFPREGS	15
#endif  
