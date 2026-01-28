#ifndef _BPF_JIT_COMP_H
#define _BPF_JIT_COMP_H
#define MIPS_R_ZERO	0    
#define MIPS_R_AT	1    
#define MIPS_R_V0	2    
#define MIPS_R_V1	3    
#define MIPS_R_A0	4    
#define MIPS_R_A1	5    
#define MIPS_R_A2	6    
#define MIPS_R_A3	7    
#define MIPS_R_A4	8    
#define MIPS_R_A5	9    
#define MIPS_R_A6	10   
#define MIPS_R_A7	11   
#define MIPS_R_T0	8    
#define MIPS_R_T1	9    
#define MIPS_R_T2	10   
#define MIPS_R_T3	11   
#define MIPS_R_T4	12   
#define MIPS_R_T5	13   
#define MIPS_R_T6	14   
#define MIPS_R_T7	15   
#define MIPS_R_S0	16   
#define MIPS_R_S1	17   
#define MIPS_R_S2	18   
#define MIPS_R_S3	19   
#define MIPS_R_S4	20   
#define MIPS_R_S5	21   
#define MIPS_R_S6	22   
#define MIPS_R_S7	23   
#define MIPS_R_T8	24   
#define MIPS_R_T9	25   
#define MIPS_R_GP	28   
#define MIPS_R_SP	29   
#define MIPS_R_FP	30   
#define MIPS_R_RA	31   
#define MIPS_JMP_MASK	0x0fffffffUL
#define JIT_MAX_ITERATIONS	8
#define JIT_JNSET	0xe0
#define JIT_JNOP	0xf0
#define JIT_DESC_CONVERT	BIT(31)
struct jit_context {
	struct bpf_prog *program;      
	u32 *descriptors;              
	u32 *target;                   
	u32 bpf_index;                 
	u32 jit_index;                 
	u32 changes;                   
	u32 accessed;                  
	u32 clobbered;                 
	u32 stack_size;                
	u32 saved_size;                
	u32 stack_used;                
};
#define __emit(ctx, func, ...)					\
do {								\
	if ((ctx)->target != NULL) {				\
		u32 *p = &(ctx)->target[ctx->jit_index];	\
		uasm_i_##func(&p, ##__VA_ARGS__);		\
	}							\
	(ctx)->jit_index++;					\
} while (0)
#define emit(...) __emit(__VA_ARGS__)
#ifdef CONFIG_WAR_R10000_LLSC
#define LLSC_beqz	beqzl
#else
#define LLSC_beqz	beqz
#endif
#ifdef CONFIG_CPU_LOONGSON3_WORKAROUNDS
#define LLSC_sync(ctx)	emit(ctx, sync, 0)
#define LLSC_offset	4
#else
#define LLSC_sync(ctx)
#define LLSC_offset	0
#endif
#ifdef CONFIG_CPU_JUMP_WORKAROUNDS
#define JALR_MASK	0xffffffffcfffffffULL
#else
#define JALR_MASK	(~0ULL)
#endif
static inline void access_reg(struct jit_context *ctx, u8 reg)
{
	ctx->accessed |= BIT(reg);
}
static inline void clobber_reg(struct jit_context *ctx, u8 reg)
{
	ctx->clobbered |= BIT(reg);
}
int push_regs(struct jit_context *ctx, u32 mask, u32 excl, int depth);
int pop_regs(struct jit_context *ctx, u32 mask, u32 excl, int depth);
int get_target(struct jit_context *ctx, u32 loc);
int get_offset(const struct jit_context *ctx, int off);
void emit_mov_i(struct jit_context *ctx, u8 dst, s32 imm);
void emit_mov_r(struct jit_context *ctx, u8 dst, u8 src);
bool valid_alu_i(u8 op, s32 imm);
bool rewrite_alu_i(u8 op, s32 imm, u8 *alu, s32 *val);
void emit_alu_i(struct jit_context *ctx, u8 dst, s32 imm, u8 op);
void emit_alu_r(struct jit_context *ctx, u8 dst, u8 src, u8 op);
void emit_atomic_r(struct jit_context *ctx, u8 dst, u8 src, s16 off, u8 code);
void emit_cmpxchg_r(struct jit_context *ctx, u8 dst, u8 src, u8 res, s16 off);
void emit_bswap_r(struct jit_context *ctx, u8 dst, u32 width);
bool valid_jmp_i(u8 op, s32 imm);
void setup_jmp_i(struct jit_context *ctx, s32 imm, u8 width,
		 u8 bpf_op, s16 bpf_off, u8 *jit_op, s32 *jit_off);
void setup_jmp_r(struct jit_context *ctx, bool same_reg,
		 u8 bpf_op, s16 bpf_off, u8 *jit_op, s32 *jit_off);
int finish_jmp(struct jit_context *ctx, u8 jit_op, s16 bpf_off);
void emit_jmp_i(struct jit_context *ctx, u8 dst, s32 imm, s32 off, u8 op);
void emit_jmp_r(struct jit_context *ctx, u8 dst, u8 src, s32 off, u8 op);
int emit_ja(struct jit_context *ctx, s16 off);
int emit_exit(struct jit_context *ctx);
void build_prologue(struct jit_context *ctx);
void build_epilogue(struct jit_context *ctx, int dest_reg);
int build_insn(const struct bpf_insn *insn, struct jit_context *ctx);
#endif  
