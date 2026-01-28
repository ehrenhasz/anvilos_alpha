#ifndef _ASM_S390_FPU_TYPES_H
#define _ASM_S390_FPU_TYPES_H
#include <asm/sigcontext.h>
struct fpu {
	__u32 fpc;		 
	void *regs;		 
	union {
		freg_t fprs[__NUM_FPRS];
		__vector128 vxrs[__NUM_VXRS];
	};
};
struct vx_array { __vector128 _[__NUM_VXRS]; };
struct kernel_fpu {
	u32	    mask;
	u32	    fpc;
	union {
		freg_t fprs[__NUM_FPRS];
		__vector128 vxrs[__NUM_VXRS];
	};
};
#endif  
