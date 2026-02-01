 
 
#ifndef _ASM_X86_FPU_H
#define _ASM_X86_FPU_H

 
struct fregs_state {
	u32			cwd;	 
	u32			swd;	 
	u32			twd;	 
	u32			fip;	 
	u32			fcs;	 
	u32			foo;	 
	u32			fos;	 

	 
	u32			st_space[20];

	 
	u32			status;
};

 
struct fxregs_state {
	u16			cwd;  
	u16			swd;  
	u16			twd;  
	u16			fop;  
	union {
		struct {
			u64	rip;  
			u64	rdp;  
		};
		struct {
			u32	fip;  
			u32	fcs;  
			u32	foo;  
			u32	fos;  
		};
	};
	u32			mxcsr;		 
	u32			mxcsr_mask;	 

	 
	u32			st_space[32];

	 
	u32			xmm_space[64];

	u32			padding[12];

	union {
		u32		padding1[12];
		u32		sw_reserved[12];
	};

} __attribute__((aligned(16)));

 
#define MXCSR_DEFAULT		0x1f80

 
#define MXCSR_AND_FLAGS_SIZE sizeof(u64)

 
struct swregs_state {
	u32			cwd;
	u32			swd;
	u32			twd;
	u32			fip;
	u32			fcs;
	u32			foo;
	u32			fos;
	 
	u32			st_space[20];
	u8			ftop;
	u8			changed;
	u8			lookahead;
	u8			no_update;
	u8			rm;
	u8			alimit;
	struct math_emu_info	*info;
	u32			entry_eip;
};

 
enum xfeature {
	XFEATURE_FP,
	XFEATURE_SSE,
	 
	XFEATURE_YMM,
	XFEATURE_BNDREGS,
	XFEATURE_BNDCSR,
	XFEATURE_OPMASK,
	XFEATURE_ZMM_Hi256,
	XFEATURE_Hi16_ZMM,
	XFEATURE_PT_UNIMPLEMENTED_SO_FAR,
	XFEATURE_PKRU,
	XFEATURE_PASID,
	XFEATURE_CET_USER,
	XFEATURE_CET_KERNEL_UNUSED,
	XFEATURE_RSRVD_COMP_13,
	XFEATURE_RSRVD_COMP_14,
	XFEATURE_LBR,
	XFEATURE_RSRVD_COMP_16,
	XFEATURE_XTILE_CFG,
	XFEATURE_XTILE_DATA,

	XFEATURE_MAX,
};

#define XFEATURE_MASK_FP		(1 << XFEATURE_FP)
#define XFEATURE_MASK_SSE		(1 << XFEATURE_SSE)
#define XFEATURE_MASK_YMM		(1 << XFEATURE_YMM)
#define XFEATURE_MASK_BNDREGS		(1 << XFEATURE_BNDREGS)
#define XFEATURE_MASK_BNDCSR		(1 << XFEATURE_BNDCSR)
#define XFEATURE_MASK_OPMASK		(1 << XFEATURE_OPMASK)
#define XFEATURE_MASK_ZMM_Hi256		(1 << XFEATURE_ZMM_Hi256)
#define XFEATURE_MASK_Hi16_ZMM		(1 << XFEATURE_Hi16_ZMM)
#define XFEATURE_MASK_PT		(1 << XFEATURE_PT_UNIMPLEMENTED_SO_FAR)
#define XFEATURE_MASK_PKRU		(1 << XFEATURE_PKRU)
#define XFEATURE_MASK_PASID		(1 << XFEATURE_PASID)
#define XFEATURE_MASK_CET_USER		(1 << XFEATURE_CET_USER)
#define XFEATURE_MASK_CET_KERNEL	(1 << XFEATURE_CET_KERNEL_UNUSED)
#define XFEATURE_MASK_LBR		(1 << XFEATURE_LBR)
#define XFEATURE_MASK_XTILE_CFG		(1 << XFEATURE_XTILE_CFG)
#define XFEATURE_MASK_XTILE_DATA	(1 << XFEATURE_XTILE_DATA)

#define XFEATURE_MASK_FPSSE		(XFEATURE_MASK_FP | XFEATURE_MASK_SSE)
#define XFEATURE_MASK_AVX512		(XFEATURE_MASK_OPMASK \
					 | XFEATURE_MASK_ZMM_Hi256 \
					 | XFEATURE_MASK_Hi16_ZMM)

#ifdef CONFIG_X86_64
# define XFEATURE_MASK_XTILE		(XFEATURE_MASK_XTILE_DATA \
					 | XFEATURE_MASK_XTILE_CFG)
#else
# define XFEATURE_MASK_XTILE		(0)
#endif

#define FIRST_EXTENDED_XFEATURE	XFEATURE_YMM

struct reg_128_bit {
	u8      regbytes[128/8];
};
struct reg_256_bit {
	u8	regbytes[256/8];
};
struct reg_512_bit {
	u8	regbytes[512/8];
};
struct reg_1024_byte {
	u8	regbytes[1024];
};

 
struct ymmh_struct {
	struct reg_128_bit              hi_ymm[16];
} __packed;

 

struct mpx_bndreg {
	u64				lower_bound;
	u64				upper_bound;
} __packed;
 
struct mpx_bndreg_state {
	struct mpx_bndreg		bndreg[4];
} __packed;

 
struct mpx_bndcsr {
	u64				bndcfgu;
	u64				bndstatus;
} __packed;

 
struct mpx_bndcsr_state {
	union {
		struct mpx_bndcsr		bndcsr;
		u8				pad_to_64_bytes[64];
	};
} __packed;

 

 
struct avx_512_opmask_state {
	u64				opmask_reg[8];
} __packed;

 
struct avx_512_zmm_uppers_state {
	struct reg_256_bit		zmm_upper[16];
} __packed;

 
struct avx_512_hi16_state {
	struct reg_512_bit		hi16_zmm[16];
} __packed;

 
struct pkru_state {
	u32				pkru;
	u32				pad;
} __packed;

 
struct cet_user_state {
	 
	u64 user_cet;
	 
	u64 user_ssp;
};

 

struct lbr_entry {
	u64 from;
	u64 to;
	u64 info;
};

struct arch_lbr_state {
	u64 lbr_ctl;
	u64 lbr_depth;
	u64 ler_from;
	u64 ler_to;
	u64 ler_info;
	struct lbr_entry		entries[];
};

 
struct xtile_cfg {
	u64				tcfg[8];
} __packed;

 
struct xtile_data {
	struct reg_1024_byte		tmm;
} __packed;

 
struct ia32_pasid_state {
	u64 pasid;
} __packed;

struct xstate_header {
	u64				xfeatures;
	u64				xcomp_bv;
	u64				reserved[6];
} __attribute__((packed));

 
#define XCOMP_BV_COMPACTED_FORMAT ((u64)1 << 63)

 
struct xregs_state {
	struct fxregs_state		i387;
	struct xstate_header		header;
	u8				extended_state_area[];
} __attribute__ ((packed, aligned (64)));

 
union fpregs_state {
	struct fregs_state		fsave;
	struct fxregs_state		fxsave;
	struct swregs_state		soft;
	struct xregs_state		xsave;
	u8 __padding[PAGE_SIZE];
};

struct fpstate {
	 
	unsigned int		size;

	 
	unsigned int		user_size;

	 
	u64			xfeatures;

	 
	u64			user_xfeatures;

	 
	u64			xfd;

	 
	unsigned int		is_valloc	: 1;

	 
	unsigned int		is_guest	: 1;

	 
	unsigned int		is_confidential	: 1;

	 
	unsigned int		in_use		: 1;

	 
	union fpregs_state	regs;

	 
} __aligned(64);

#define FPU_GUEST_PERM_LOCKED		BIT_ULL(63)

struct fpu_state_perm {
	 
	u64				__state_perm;

	 
	unsigned int			__state_size;

	 
	unsigned int			__user_state_size;
};

 
struct fpu {
	 
	unsigned int			last_cpu;

	 
	unsigned long			avx512_timestamp;

	 
	struct fpstate			*fpstate;

	 
	struct fpstate			*__task_fpstate;

	 
	struct fpu_state_perm		perm;

	 
	struct fpu_state_perm		guest_perm;

	 
	struct fpstate			__fpstate;
	 
};

 
struct fpu_guest {
	 
	u64				xfeatures;

	 
	u64				perm;

	 
	u64				xfd_err;

	 
	unsigned int			uabi_size;

	 
	struct fpstate			*fpstate;
};

 
struct fpu_state_config {
	 
	unsigned int		max_size;

	 
	unsigned int		default_size;

	 
	u64 max_features;

	 
	u64 default_features;
	 
	u64 legacy_features;
};

 
extern struct fpu_state_config fpu_kernel_cfg, fpu_user_cfg;

#endif  
