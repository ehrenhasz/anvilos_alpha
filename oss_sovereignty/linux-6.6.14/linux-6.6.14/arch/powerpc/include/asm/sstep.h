#include <asm/inst.h>
struct pt_regs;
#define IS_MTMSRD(instr)	((ppc_inst_val(instr) & 0xfc0007be) == 0x7c000124)
#define IS_RFID(instr)		((ppc_inst_val(instr) & 0xfc0007be) == 0x4c000024)
enum instruction_type {
	COMPUTE,		 
	LOAD,			 
	LOAD_MULTI,
	LOAD_FP,
	LOAD_VMX,
	LOAD_VSX,
	STORE,
	STORE_MULTI,
	STORE_FP,
	STORE_VMX,
	STORE_VSX,
	LARX,
	STCX,
	BRANCH,
	MFSPR,
	MTSPR,
	CACHEOP,
	BARRIER,
	SYSCALL,
	SYSCALL_VECTORED_0,
	MFMSR,
	MTMSR,
	RFI,
	INTERRUPT,
	UNKNOWN
};
#define INSTR_TYPE_MASK	0x1f
#define OP_IS_LOAD(type)	((LOAD <= (type) && (type) <= LOAD_VSX) || (type) == LARX)
#define OP_IS_STORE(type)	((STORE <= (type) && (type) <= STORE_VSX) || (type) == STCX)
#define OP_IS_LOAD_STORE(type)	(LOAD <= (type) && (type) <= STCX)
#define SETREG		0x20
#define SETCC		0x40
#define SETXER		0x80
#define SETLK		0x20
#define BRTAKEN		0x40
#define DECCTR		0x80
#define SIGNEXT		0x20
#define UPDATE		0x40	 
#define BYTEREV		0x80
#define FPCONV		0x100
#define BARRIER_MASK	0xe0
#define BARRIER_SYNC	0x00
#define BARRIER_ISYNC	0x20
#define BARRIER_EIEIO	0x40
#define BARRIER_LWSYNC	0x60
#define BARRIER_PTESYNC	0x80
#define CACHEOP_MASK	0x700
#define DCBST		0
#define DCBF		0x100
#define DCBTST		0x200
#define DCBT		0x300
#define ICBI		0x400
#define DCBZ		0x500
#define VSX_FPCONV	1	 
#define VSX_SPLAT	2	 
#define VSX_LDLEFT	4	 
#define VSX_CHECK_VEC	8	 
#define PREFIXED       0x800
#define SIZE(n)		((n) << 12)
#define GETSIZE(w)	((w) >> 12)
#define GETTYPE(t)	((t) & INSTR_TYPE_MASK)
#define GETLENGTH(t)   (((t) & PREFIXED) ? 8 : 4)
#define MKOP(t, f, s)	((t) | (f) | SIZE(s))
#define GET_PREFIX_RA(i)	(((i) >> 16) & 0x1f)
#define GET_PREFIX_R(i)		((i) & (1ul << 20))
extern s32 patch__exec_instr;
struct instruction_op {
	int type;
	int reg;
	unsigned long val;
	unsigned long ea;
	int update_reg;
	int spr;
	u32 ccval;
	u32 xerval;
	u8 element_size;	 
	u8 vsx_flags;
};
union vsx_reg {
	u8	b[16];
	u16	h[8];
	u32	w[4];
	unsigned long d[2];
	float	fp[4];
	double	dp[2];
	__vector128 v;
};
extern int analyse_instr(struct instruction_op *op, const struct pt_regs *regs,
			 ppc_inst_t instr);
void emulate_update_regs(struct pt_regs *reg, struct instruction_op *op);
int emulate_step(struct pt_regs *regs, ppc_inst_t instr);
extern int emulate_loadstore(struct pt_regs *regs, struct instruction_op *op);
extern void emulate_vsx_load(struct instruction_op *op, union vsx_reg *reg,
			     const void *mem, bool cross_endian);
extern void emulate_vsx_store(struct instruction_op *op,
			      const union vsx_reg *reg, void *mem,
			      bool cross_endian);
extern int emulate_dcbz(unsigned long ea, struct pt_regs *regs);
