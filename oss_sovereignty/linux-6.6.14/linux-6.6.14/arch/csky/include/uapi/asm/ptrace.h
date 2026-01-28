#ifndef _CSKY_PTRACE_H
#define _CSKY_PTRACE_H
#ifndef __ASSEMBLY__
struct pt_regs {
	unsigned long	tls;
	unsigned long	lr;
	unsigned long	pc;
	unsigned long	sr;
	unsigned long	usp;
	unsigned long	orig_a0;
	unsigned long	a0;
	unsigned long	a1;
	unsigned long	a2;
	unsigned long	a3;
	unsigned long	regs[10];
#if defined(__CSKYABIV2__)
	unsigned long	exregs[15];
	unsigned long	rhi;
	unsigned long	rlo;
	unsigned long	dcsr;
#endif
};
struct user_fp {
	unsigned long	vr[96];
	unsigned long	fcr;
	unsigned long	fesr;
	unsigned long	fid;
	unsigned long	reserved;
};
#endif  
#endif  
