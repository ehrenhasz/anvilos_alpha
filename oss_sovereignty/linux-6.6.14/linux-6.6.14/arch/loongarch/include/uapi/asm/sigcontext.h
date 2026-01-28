#ifndef _UAPI_ASM_SIGCONTEXT_H
#define _UAPI_ASM_SIGCONTEXT_H
#include <linux/types.h>
#include <linux/posix_types.h>
#define SC_USED_FP		(1 << 0)
#define SC_ADDRERR_RD		(1 << 30)
#define SC_ADDRERR_WR		(1 << 31)
struct sigcontext {
	__u64	sc_pc;
	__u64	sc_regs[32];
	__u32	sc_flags;
	__u64	sc_extcontext[0] __attribute__((__aligned__(16)));
};
#define CONTEXT_INFO_ALIGN	16
struct sctx_info {
	__u32	magic;
	__u32	size;
	__u64	padding;	 
};
#define FPU_CTX_MAGIC		0x46505501
#define FPU_CTX_ALIGN		8
struct fpu_context {
	__u64	regs[32];
	__u64	fcc;
	__u32	fcsr;
};
#define LSX_CTX_MAGIC		0x53580001
#define LSX_CTX_ALIGN		16
struct lsx_context {
	__u64	regs[2*32];
	__u64	fcc;
	__u32	fcsr;
};
#define LASX_CTX_MAGIC		0x41535801
#define LASX_CTX_ALIGN		32
struct lasx_context {
	__u64	regs[4*32];
	__u64	fcc;
	__u32	fcsr;
};
#define LBT_CTX_MAGIC		0x42540001
#define LBT_CTX_ALIGN		8
struct lbt_context {
	__u64	regs[4];
	__u32	eflags;
	__u32	ftop;
};
#endif  
