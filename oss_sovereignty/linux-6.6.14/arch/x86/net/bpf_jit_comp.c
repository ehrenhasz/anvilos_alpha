
 
#include <linux/netdevice.h>
#include <linux/filter.h>
#include <linux/if_vlan.h>
#include <linux/bpf.h>
#include <linux/memory.h>
#include <linux/sort.h>
#include <asm/extable.h>
#include <asm/ftrace.h>
#include <asm/set_memory.h>
#include <asm/nospec-branch.h>
#include <asm/text-patching.h>

static u8 *emit_code(u8 *ptr, u32 bytes, unsigned int len)
{
	if (len == 1)
		*ptr = bytes;
	else if (len == 2)
		*(u16 *)ptr = bytes;
	else {
		*(u32 *)ptr = bytes;
		barrier();
	}
	return ptr + len;
}

#define EMIT(bytes, len) \
	do { prog = emit_code(prog, bytes, len); } while (0)

#define EMIT1(b1)		EMIT(b1, 1)
#define EMIT2(b1, b2)		EMIT((b1) + ((b2) << 8), 2)
#define EMIT3(b1, b2, b3)	EMIT((b1) + ((b2) << 8) + ((b3) << 16), 3)
#define EMIT4(b1, b2, b3, b4)   EMIT((b1) + ((b2) << 8) + ((b3) << 16) + ((b4) << 24), 4)

#define EMIT1_off32(b1, off) \
	do { EMIT1(b1); EMIT(off, 4); } while (0)
#define EMIT2_off32(b1, b2, off) \
	do { EMIT2(b1, b2); EMIT(off, 4); } while (0)
#define EMIT3_off32(b1, b2, b3, off) \
	do { EMIT3(b1, b2, b3); EMIT(off, 4); } while (0)
#define EMIT4_off32(b1, b2, b3, b4, off) \
	do { EMIT4(b1, b2, b3, b4); EMIT(off, 4); } while (0)

#ifdef CONFIG_X86_KERNEL_IBT
#define EMIT_ENDBR()	EMIT(gen_endbr(), 4)
#else
#define EMIT_ENDBR()
#endif

static bool is_imm8(int value)
{
	return value <= 127 && value >= -128;
}

static bool is_simm32(s64 value)
{
	return value == (s64)(s32)value;
}

static bool is_uimm32(u64 value)
{
	return value == (u64)(u32)value;
}

 
#define EMIT_mov(DST, SRC)								 \
	do {										 \
		if (DST != SRC)								 \
			EMIT3(add_2mod(0x48, DST, SRC), 0x89, add_2reg(0xC0, DST, SRC)); \
	} while (0)

static int bpf_size_to_x86_bytes(int bpf_size)
{
	if (bpf_size == BPF_W)
		return 4;
	else if (bpf_size == BPF_H)
		return 2;
	else if (bpf_size == BPF_B)
		return 1;
	else if (bpf_size == BPF_DW)
		return 4;  
	else
		return 0;
}

 
#define X86_JB  0x72
#define X86_JAE 0x73
#define X86_JE  0x74
#define X86_JNE 0x75
#define X86_JBE 0x76
#define X86_JA  0x77
#define X86_JL  0x7C
#define X86_JGE 0x7D
#define X86_JLE 0x7E
#define X86_JG  0x7F

 
#define AUX_REG (MAX_BPF_JIT_REG + 1)
#define X86_REG_R9 (MAX_BPF_JIT_REG + 2)

 
static const int reg2hex[] = {
	[BPF_REG_0] = 0,   
	[BPF_REG_1] = 7,   
	[BPF_REG_2] = 6,   
	[BPF_REG_3] = 2,   
	[BPF_REG_4] = 1,   
	[BPF_REG_5] = 0,   
	[BPF_REG_6] = 3,   
	[BPF_REG_7] = 5,   
	[BPF_REG_8] = 6,   
	[BPF_REG_9] = 7,   
	[BPF_REG_FP] = 5,  
	[BPF_REG_AX] = 2,  
	[AUX_REG] = 3,     
	[X86_REG_R9] = 1,  
};

static const int reg2pt_regs[] = {
	[BPF_REG_0] = offsetof(struct pt_regs, ax),
	[BPF_REG_1] = offsetof(struct pt_regs, di),
	[BPF_REG_2] = offsetof(struct pt_regs, si),
	[BPF_REG_3] = offsetof(struct pt_regs, dx),
	[BPF_REG_4] = offsetof(struct pt_regs, cx),
	[BPF_REG_5] = offsetof(struct pt_regs, r8),
	[BPF_REG_6] = offsetof(struct pt_regs, bx),
	[BPF_REG_7] = offsetof(struct pt_regs, r13),
	[BPF_REG_8] = offsetof(struct pt_regs, r14),
	[BPF_REG_9] = offsetof(struct pt_regs, r15),
};

 
static bool is_ereg(u32 reg)
{
	return (1 << reg) & (BIT(BPF_REG_5) |
			     BIT(AUX_REG) |
			     BIT(BPF_REG_7) |
			     BIT(BPF_REG_8) |
			     BIT(BPF_REG_9) |
			     BIT(X86_REG_R9) |
			     BIT(BPF_REG_AX));
}

 
static bool is_ereg_8l(u32 reg)
{
	return is_ereg(reg) ||
	    (1 << reg) & (BIT(BPF_REG_1) |
			  BIT(BPF_REG_2) |
			  BIT(BPF_REG_FP));
}

static bool is_axreg(u32 reg)
{
	return reg == BPF_REG_0;
}

 
static u8 add_1mod(u8 byte, u32 reg)
{
	if (is_ereg(reg))
		byte |= 1;
	return byte;
}

static u8 add_2mod(u8 byte, u32 r1, u32 r2)
{
	if (is_ereg(r1))
		byte |= 1;
	if (is_ereg(r2))
		byte |= 4;
	return byte;
}

 
static u8 add_1reg(u8 byte, u32 dst_reg)
{
	return byte + reg2hex[dst_reg];
}

 
static u8 add_2reg(u8 byte, u32 dst_reg, u32 src_reg)
{
	return byte + reg2hex[dst_reg] + (reg2hex[src_reg] << 3);
}

 
static u8 simple_alu_opcodes[] = {
	[BPF_ADD] = 0x01,
	[BPF_SUB] = 0x29,
	[BPF_AND] = 0x21,
	[BPF_OR] = 0x09,
	[BPF_XOR] = 0x31,
	[BPF_LSH] = 0xE0,
	[BPF_RSH] = 0xE8,
	[BPF_ARSH] = 0xF8,
};

static void jit_fill_hole(void *area, unsigned int size)
{
	 
	memset(area, 0xcc, size);
}

int bpf_arch_text_invalidate(void *dst, size_t len)
{
	return IS_ERR_OR_NULL(text_poke_set(dst, 0xcc, len));
}

struct jit_context {
	int cleanup_addr;  

	 
	int tail_call_direct_label;
	int tail_call_indirect_label;
};

 
#define BPF_MAX_INSN_SIZE	128
#define BPF_INSN_SAFETY		64

 
#define X86_PATCH_SIZE		5
 
#define X86_TAIL_CALL_OFFSET	(11 + ENDBR_INSN_SIZE)

static void push_callee_regs(u8 **pprog, bool *callee_regs_used)
{
	u8 *prog = *pprog;

	if (callee_regs_used[0])
		EMIT1(0x53);          
	if (callee_regs_used[1])
		EMIT2(0x41, 0x55);    
	if (callee_regs_used[2])
		EMIT2(0x41, 0x56);    
	if (callee_regs_used[3])
		EMIT2(0x41, 0x57);    
	*pprog = prog;
}

static void pop_callee_regs(u8 **pprog, bool *callee_regs_used)
{
	u8 *prog = *pprog;

	if (callee_regs_used[3])
		EMIT2(0x41, 0x5F);    
	if (callee_regs_used[2])
		EMIT2(0x41, 0x5E);    
	if (callee_regs_used[1])
		EMIT2(0x41, 0x5D);    
	if (callee_regs_used[0])
		EMIT1(0x5B);          
	*pprog = prog;
}

 
static void emit_prologue(u8 **pprog, u32 stack_depth, bool ebpf_from_cbpf,
			  bool tail_call_reachable, bool is_subprog)
{
	u8 *prog = *pprog;

	 
	EMIT_ENDBR();
	memcpy(prog, x86_nops[5], X86_PATCH_SIZE);
	prog += X86_PATCH_SIZE;
	if (!ebpf_from_cbpf) {
		if (tail_call_reachable && !is_subprog)
			EMIT2(0x31, 0xC0);  
		else
			EMIT2(0x66, 0x90);  
	}
	EMIT1(0x55);              
	EMIT3(0x48, 0x89, 0xE5);  

	 
	EMIT_ENDBR();

	 
	if (stack_depth)
		EMIT3_off32(0x48, 0x81, 0xEC, round_up(stack_depth, 8));
	if (tail_call_reachable)
		EMIT1(0x50);          
	*pprog = prog;
}

static int emit_patch(u8 **pprog, void *func, void *ip, u8 opcode)
{
	u8 *prog = *pprog;
	s64 offset;

	offset = func - (ip + X86_PATCH_SIZE);
	if (!is_simm32(offset)) {
		pr_err("Target call %p is out of range\n", func);
		return -ERANGE;
	}
	EMIT1_off32(opcode, offset);
	*pprog = prog;
	return 0;
}

static int emit_call(u8 **pprog, void *func, void *ip)
{
	return emit_patch(pprog, func, ip, 0xE8);
}

static int emit_rsb_call(u8 **pprog, void *func, void *ip)
{
	OPTIMIZER_HIDE_VAR(func);
	x86_call_depth_emit_accounting(pprog, func);
	return emit_patch(pprog, func, ip, 0xE8);
}

static int emit_jump(u8 **pprog, void *func, void *ip)
{
	return emit_patch(pprog, func, ip, 0xE9);
}

static int __bpf_arch_text_poke(void *ip, enum bpf_text_poke_type t,
				void *old_addr, void *new_addr)
{
	const u8 *nop_insn = x86_nops[5];
	u8 old_insn[X86_PATCH_SIZE];
	u8 new_insn[X86_PATCH_SIZE];
	u8 *prog;
	int ret;

	memcpy(old_insn, nop_insn, X86_PATCH_SIZE);
	if (old_addr) {
		prog = old_insn;
		ret = t == BPF_MOD_CALL ?
		      emit_call(&prog, old_addr, ip) :
		      emit_jump(&prog, old_addr, ip);
		if (ret)
			return ret;
	}

	memcpy(new_insn, nop_insn, X86_PATCH_SIZE);
	if (new_addr) {
		prog = new_insn;
		ret = t == BPF_MOD_CALL ?
		      emit_call(&prog, new_addr, ip) :
		      emit_jump(&prog, new_addr, ip);
		if (ret)
			return ret;
	}

	ret = -EBUSY;
	mutex_lock(&text_mutex);
	if (memcmp(ip, old_insn, X86_PATCH_SIZE))
		goto out;
	ret = 1;
	if (memcmp(ip, new_insn, X86_PATCH_SIZE)) {
		text_poke_bp(ip, new_insn, X86_PATCH_SIZE, NULL);
		ret = 0;
	}
out:
	mutex_unlock(&text_mutex);
	return ret;
}

int bpf_arch_text_poke(void *ip, enum bpf_text_poke_type t,
		       void *old_addr, void *new_addr)
{
	if (!is_kernel_text((long)ip) &&
	    !is_bpf_text_address((long)ip))
		 
		return -EINVAL;

	 
	if (is_endbr(*(u32 *)ip))
		ip += ENDBR_INSN_SIZE;

	return __bpf_arch_text_poke(ip, t, old_addr, new_addr);
}

#define EMIT_LFENCE()	EMIT3(0x0F, 0xAE, 0xE8)

static void emit_indirect_jump(u8 **pprog, int reg, u8 *ip)
{
	u8 *prog = *pprog;

	if (cpu_feature_enabled(X86_FEATURE_RETPOLINE_LFENCE)) {
		EMIT_LFENCE();
		EMIT2(0xFF, 0xE0 + reg);
	} else if (cpu_feature_enabled(X86_FEATURE_RETPOLINE)) {
		OPTIMIZER_HIDE_VAR(reg);
		if (cpu_feature_enabled(X86_FEATURE_CALL_DEPTH))
			emit_jump(&prog, &__x86_indirect_jump_thunk_array[reg], ip);
		else
			emit_jump(&prog, &__x86_indirect_thunk_array[reg], ip);
	} else {
		EMIT2(0xFF, 0xE0 + reg);	 
		if (IS_ENABLED(CONFIG_RETPOLINE) || IS_ENABLED(CONFIG_SLS))
			EMIT1(0xCC);		 
	}

	*pprog = prog;
}

static void emit_return(u8 **pprog, u8 *ip)
{
	u8 *prog = *pprog;

	if (cpu_feature_enabled(X86_FEATURE_RETHUNK)) {
		emit_jump(&prog, x86_return_thunk, ip);
	} else {
		EMIT1(0xC3);		 
		if (IS_ENABLED(CONFIG_SLS))
			EMIT1(0xCC);	 
	}

	*pprog = prog;
}

 
static void emit_bpf_tail_call_indirect(u8 **pprog, bool *callee_regs_used,
					u32 stack_depth, u8 *ip,
					struct jit_context *ctx)
{
	int tcc_off = -4 - round_up(stack_depth, 8);
	u8 *prog = *pprog, *start = *pprog;
	int offset;

	 

	 
	EMIT2(0x89, 0xD2);                         
	EMIT3(0x39, 0x56,                          
	      offsetof(struct bpf_array, map.max_entries));

	offset = ctx->tail_call_indirect_label - (prog + 2 - start);
	EMIT2(X86_JBE, offset);                    

	 
	EMIT2_off32(0x8B, 0x85, tcc_off);          
	EMIT3(0x83, 0xF8, MAX_TAIL_CALL_CNT);      

	offset = ctx->tail_call_indirect_label - (prog + 2 - start);
	EMIT2(X86_JAE, offset);                    
	EMIT3(0x83, 0xC0, 0x01);                   
	EMIT2_off32(0x89, 0x85, tcc_off);          

	 
	EMIT4_off32(0x48, 0x8B, 0x8C, 0xD6,        
		    offsetof(struct bpf_array, ptrs));

	 
	EMIT3(0x48, 0x85, 0xC9);                   

	offset = ctx->tail_call_indirect_label - (prog + 2 - start);
	EMIT2(X86_JE, offset);                     

	pop_callee_regs(&prog, callee_regs_used);

	EMIT1(0x58);                               
	if (stack_depth)
		EMIT3_off32(0x48, 0x81, 0xC4,      
			    round_up(stack_depth, 8));

	 
	EMIT4(0x48, 0x8B, 0x49,                    
	      offsetof(struct bpf_prog, bpf_func));
	EMIT4(0x48, 0x83, 0xC1,                    
	      X86_TAIL_CALL_OFFSET);
	 
	emit_indirect_jump(&prog, 1  , ip + (prog - start));

	 
	ctx->tail_call_indirect_label = prog - start;
	*pprog = prog;
}

static void emit_bpf_tail_call_direct(struct bpf_jit_poke_descriptor *poke,
				      u8 **pprog, u8 *ip,
				      bool *callee_regs_used, u32 stack_depth,
				      struct jit_context *ctx)
{
	int tcc_off = -4 - round_up(stack_depth, 8);
	u8 *prog = *pprog, *start = *pprog;
	int offset;

	 
	EMIT2_off32(0x8B, 0x85, tcc_off);              
	EMIT3(0x83, 0xF8, MAX_TAIL_CALL_CNT);          

	offset = ctx->tail_call_direct_label - (prog + 2 - start);
	EMIT2(X86_JAE, offset);                        
	EMIT3(0x83, 0xC0, 0x01);                       
	EMIT2_off32(0x89, 0x85, tcc_off);              

	poke->tailcall_bypass = ip + (prog - start);
	poke->adj_off = X86_TAIL_CALL_OFFSET;
	poke->tailcall_target = ip + ctx->tail_call_direct_label - X86_PATCH_SIZE;
	poke->bypass_addr = (u8 *)poke->tailcall_target + X86_PATCH_SIZE;

	emit_jump(&prog, (u8 *)poke->tailcall_target + X86_PATCH_SIZE,
		  poke->tailcall_bypass);

	pop_callee_regs(&prog, callee_regs_used);
	EMIT1(0x58);                                   
	if (stack_depth)
		EMIT3_off32(0x48, 0x81, 0xC4, round_up(stack_depth, 8));

	memcpy(prog, x86_nops[5], X86_PATCH_SIZE);
	prog += X86_PATCH_SIZE;

	 
	ctx->tail_call_direct_label = prog - start;

	*pprog = prog;
}

static void bpf_tail_call_direct_fixup(struct bpf_prog *prog)
{
	struct bpf_jit_poke_descriptor *poke;
	struct bpf_array *array;
	struct bpf_prog *target;
	int i, ret;

	for (i = 0; i < prog->aux->size_poke_tab; i++) {
		poke = &prog->aux->poke_tab[i];
		if (poke->aux && poke->aux != prog->aux)
			continue;

		WARN_ON_ONCE(READ_ONCE(poke->tailcall_target_stable));

		if (poke->reason != BPF_POKE_REASON_TAIL_CALL)
			continue;

		array = container_of(poke->tail_call.map, struct bpf_array, map);
		mutex_lock(&array->aux->poke_mutex);
		target = array->ptrs[poke->tail_call.key];
		if (target) {
			ret = __bpf_arch_text_poke(poke->tailcall_target,
						   BPF_MOD_JUMP, NULL,
						   (u8 *)target->bpf_func +
						   poke->adj_off);
			BUG_ON(ret < 0);
			ret = __bpf_arch_text_poke(poke->tailcall_bypass,
						   BPF_MOD_JUMP,
						   (u8 *)poke->tailcall_target +
						   X86_PATCH_SIZE, NULL);
			BUG_ON(ret < 0);
		}
		WRITE_ONCE(poke->tailcall_target_stable, true);
		mutex_unlock(&array->aux->poke_mutex);
	}
}

static void emit_mov_imm32(u8 **pprog, bool sign_propagate,
			   u32 dst_reg, const u32 imm32)
{
	u8 *prog = *pprog;
	u8 b1, b2, b3;

	 
	if (sign_propagate && (s32)imm32 < 0) {
		 
		b1 = add_1mod(0x48, dst_reg);
		b2 = 0xC7;
		b3 = 0xC0;
		EMIT3_off32(b1, b2, add_1reg(b3, dst_reg), imm32);
		goto done;
	}

	 
	if (imm32 == 0) {
		if (is_ereg(dst_reg))
			EMIT1(add_2mod(0x40, dst_reg, dst_reg));
		b2 = 0x31;  
		b3 = 0xC0;
		EMIT2(b2, add_2reg(b3, dst_reg, dst_reg));
		goto done;
	}

	 
	if (is_ereg(dst_reg))
		EMIT1(add_1mod(0x40, dst_reg));
	EMIT1_off32(add_1reg(0xB8, dst_reg), imm32);
done:
	*pprog = prog;
}

static void emit_mov_imm64(u8 **pprog, u32 dst_reg,
			   const u32 imm32_hi, const u32 imm32_lo)
{
	u8 *prog = *pprog;

	if (is_uimm32(((u64)imm32_hi << 32) | (u32)imm32_lo)) {
		 
		emit_mov_imm32(&prog, false, dst_reg, imm32_lo);
	} else {
		 
		EMIT2(add_1mod(0x48, dst_reg), add_1reg(0xB8, dst_reg));
		EMIT(imm32_lo, 4);
		EMIT(imm32_hi, 4);
	}

	*pprog = prog;
}

static void emit_mov_reg(u8 **pprog, bool is64, u32 dst_reg, u32 src_reg)
{
	u8 *prog = *pprog;

	if (is64) {
		 
		EMIT_mov(dst_reg, src_reg);
	} else {
		 
		if (is_ereg(dst_reg) || is_ereg(src_reg))
			EMIT1(add_2mod(0x40, dst_reg, src_reg));
		EMIT2(0x89, add_2reg(0xC0, dst_reg, src_reg));
	}

	*pprog = prog;
}

static void emit_movsx_reg(u8 **pprog, int num_bits, bool is64, u32 dst_reg,
			   u32 src_reg)
{
	u8 *prog = *pprog;

	if (is64) {
		 
		if (num_bits == 8)
			EMIT4(add_2mod(0x48, src_reg, dst_reg), 0x0f, 0xbe,
			      add_2reg(0xC0, src_reg, dst_reg));
		else if (num_bits == 16)
			EMIT4(add_2mod(0x48, src_reg, dst_reg), 0x0f, 0xbf,
			      add_2reg(0xC0, src_reg, dst_reg));
		else if (num_bits == 32)
			EMIT3(add_2mod(0x48, src_reg, dst_reg), 0x63,
			      add_2reg(0xC0, src_reg, dst_reg));
	} else {
		 
		if (num_bits == 8) {
			EMIT4(add_2mod(0x40, src_reg, dst_reg), 0x0f, 0xbe,
			      add_2reg(0xC0, src_reg, dst_reg));
		} else if (num_bits == 16) {
			if (is_ereg(dst_reg) || is_ereg(src_reg))
				EMIT1(add_2mod(0x40, src_reg, dst_reg));
			EMIT3(add_2mod(0x0f, src_reg, dst_reg), 0xbf,
			      add_2reg(0xC0, src_reg, dst_reg));
		}
	}

	*pprog = prog;
}

 
static void emit_insn_suffix(u8 **pprog, u32 ptr_reg, u32 val_reg, int off)
{
	u8 *prog = *pprog;

	if (is_imm8(off)) {
		 
		EMIT2(add_2reg(0x40, ptr_reg, val_reg), off);
	} else {
		 
		EMIT1_off32(add_2reg(0x80, ptr_reg, val_reg), off);
	}
	*pprog = prog;
}

 
static void maybe_emit_mod(u8 **pprog, u32 dst_reg, u32 src_reg, bool is64)
{
	u8 *prog = *pprog;

	if (is64)
		EMIT1(add_2mod(0x48, dst_reg, src_reg));
	else if (is_ereg(dst_reg) || is_ereg(src_reg))
		EMIT1(add_2mod(0x40, dst_reg, src_reg));
	*pprog = prog;
}

 
static void maybe_emit_1mod(u8 **pprog, u32 reg, bool is64)
{
	u8 *prog = *pprog;

	if (is64)
		EMIT1(add_1mod(0x48, reg));
	else if (is_ereg(reg))
		EMIT1(add_1mod(0x40, reg));
	*pprog = prog;
}

 
static void emit_ldx(u8 **pprog, u32 size, u32 dst_reg, u32 src_reg, int off)
{
	u8 *prog = *pprog;

	switch (size) {
	case BPF_B:
		 
		EMIT3(add_2mod(0x48, src_reg, dst_reg), 0x0F, 0xB6);
		break;
	case BPF_H:
		 
		EMIT3(add_2mod(0x48, src_reg, dst_reg), 0x0F, 0xB7);
		break;
	case BPF_W:
		 
		if (is_ereg(dst_reg) || is_ereg(src_reg))
			EMIT2(add_2mod(0x40, src_reg, dst_reg), 0x8B);
		else
			EMIT1(0x8B);
		break;
	case BPF_DW:
		 
		EMIT2(add_2mod(0x48, src_reg, dst_reg), 0x8B);
		break;
	}
	emit_insn_suffix(&prog, src_reg, dst_reg, off);
	*pprog = prog;
}

 
static void emit_ldsx(u8 **pprog, u32 size, u32 dst_reg, u32 src_reg, int off)
{
	u8 *prog = *pprog;

	switch (size) {
	case BPF_B:
		 
		EMIT3(add_2mod(0x48, src_reg, dst_reg), 0x0F, 0xBE);
		break;
	case BPF_H:
		 
		EMIT3(add_2mod(0x48, src_reg, dst_reg), 0x0F, 0xBF);
		break;
	case BPF_W:
		 
		EMIT2(add_2mod(0x48, src_reg, dst_reg), 0x63);
		break;
	}
	emit_insn_suffix(&prog, src_reg, dst_reg, off);
	*pprog = prog;
}

 
static void emit_stx(u8 **pprog, u32 size, u32 dst_reg, u32 src_reg, int off)
{
	u8 *prog = *pprog;

	switch (size) {
	case BPF_B:
		 
		if (is_ereg(dst_reg) || is_ereg_8l(src_reg))
			 
			EMIT2(add_2mod(0x40, dst_reg, src_reg), 0x88);
		else
			EMIT1(0x88);
		break;
	case BPF_H:
		if (is_ereg(dst_reg) || is_ereg(src_reg))
			EMIT3(0x66, add_2mod(0x40, dst_reg, src_reg), 0x89);
		else
			EMIT2(0x66, 0x89);
		break;
	case BPF_W:
		if (is_ereg(dst_reg) || is_ereg(src_reg))
			EMIT2(add_2mod(0x40, dst_reg, src_reg), 0x89);
		else
			EMIT1(0x89);
		break;
	case BPF_DW:
		EMIT2(add_2mod(0x48, dst_reg, src_reg), 0x89);
		break;
	}
	emit_insn_suffix(&prog, dst_reg, src_reg, off);
	*pprog = prog;
}

static int emit_atomic(u8 **pprog, u8 atomic_op,
		       u32 dst_reg, u32 src_reg, s16 off, u8 bpf_size)
{
	u8 *prog = *pprog;

	EMIT1(0xF0);  

	maybe_emit_mod(&prog, dst_reg, src_reg, bpf_size == BPF_DW);

	 
	switch (atomic_op) {
	case BPF_ADD:
	case BPF_AND:
	case BPF_OR:
	case BPF_XOR:
		 
		EMIT1(simple_alu_opcodes[atomic_op]);
		break;
	case BPF_ADD | BPF_FETCH:
		 
		EMIT2(0x0F, 0xC1);
		break;
	case BPF_XCHG:
		 
		EMIT1(0x87);
		break;
	case BPF_CMPXCHG:
		 
		EMIT2(0x0F, 0xB1);
		break;
	default:
		pr_err("bpf_jit: unknown atomic opcode %02x\n", atomic_op);
		return -EFAULT;
	}

	emit_insn_suffix(&prog, dst_reg, src_reg, off);

	*pprog = prog;
	return 0;
}

bool ex_handler_bpf(const struct exception_table_entry *x, struct pt_regs *regs)
{
	u32 reg = x->fixup >> 8;

	 
	*(unsigned long *)((void *)regs + reg) = 0;
	regs->ip += x->fixup & 0xff;
	return true;
}

static void detect_reg_usage(struct bpf_insn *insn, int insn_cnt,
			     bool *regs_used, bool *tail_call_seen)
{
	int i;

	for (i = 1; i <= insn_cnt; i++, insn++) {
		if (insn->code == (BPF_JMP | BPF_TAIL_CALL))
			*tail_call_seen = true;
		if (insn->dst_reg == BPF_REG_6 || insn->src_reg == BPF_REG_6)
			regs_used[0] = true;
		if (insn->dst_reg == BPF_REG_7 || insn->src_reg == BPF_REG_7)
			regs_used[1] = true;
		if (insn->dst_reg == BPF_REG_8 || insn->src_reg == BPF_REG_8)
			regs_used[2] = true;
		if (insn->dst_reg == BPF_REG_9 || insn->src_reg == BPF_REG_9)
			regs_used[3] = true;
	}
}

static void emit_nops(u8 **pprog, int len)
{
	u8 *prog = *pprog;
	int i, noplen;

	while (len > 0) {
		noplen = len;

		if (noplen > ASM_NOP_MAX)
			noplen = ASM_NOP_MAX;

		for (i = 0; i < noplen; i++)
			EMIT1(x86_nops[noplen][i]);
		len -= noplen;
	}

	*pprog = prog;
}

 
static void emit_3vex(u8 **pprog, bool r, bool x, bool b, u8 m,
		      bool w, u8 src_reg2, bool l, u8 pp)
{
	u8 *prog = *pprog;
	const u8 b0 = 0xc4;  
	u8 b1, b2;
	u8 vvvv = reg2hex[src_reg2];

	 
	if (is_ereg(src_reg2))
		vvvv |= 1 << 3;

	 
	b1 = (!r << 7) | (!x << 6) | (!b << 5) | (m & 0x1f);
	 
	b2 = (w << 7) | ((~vvvv & 0xf) << 3) | (l << 2) | (pp & 3);

	EMIT3(b0, b1, b2);
	*pprog = prog;
}

 
static void emit_shiftx(u8 **pprog, u32 dst_reg, u8 src_reg, bool is64, u8 op)
{
	u8 *prog = *pprog;
	bool r = is_ereg(dst_reg);
	u8 m = 2;  

	emit_3vex(&prog, r, false, r, m, is64, src_reg, false, op);
	EMIT2(0xf7, add_2reg(0xC0, dst_reg, dst_reg));
	*pprog = prog;
}

#define INSN_SZ_DIFF (((addrs[i] - addrs[i - 1]) - (prog - temp)))

 
#define RESTORE_TAIL_CALL_CNT(stack)				\
	EMIT3_off32(0x48, 0x8B, 0x85, -round_up(stack, 8) - 8)

static int do_jit(struct bpf_prog *bpf_prog, int *addrs, u8 *image, u8 *rw_image,
		  int oldproglen, struct jit_context *ctx, bool jmp_padding)
{
	bool tail_call_reachable = bpf_prog->aux->tail_call_reachable;
	struct bpf_insn *insn = bpf_prog->insnsi;
	bool callee_regs_used[4] = {};
	int insn_cnt = bpf_prog->len;
	bool tail_call_seen = false;
	bool seen_exit = false;
	u8 temp[BPF_MAX_INSN_SIZE + BPF_INSN_SAFETY];
	int i, excnt = 0;
	int ilen, proglen = 0;
	u8 *prog = temp;
	int err;

	detect_reg_usage(insn, insn_cnt, callee_regs_used,
			 &tail_call_seen);

	 
	tail_call_reachable |= tail_call_seen;

	emit_prologue(&prog, bpf_prog->aux->stack_depth,
		      bpf_prog_was_classic(bpf_prog), tail_call_reachable,
		      bpf_prog->aux->func_idx != 0);
	push_callee_regs(&prog, callee_regs_used);

	ilen = prog - temp;
	if (rw_image)
		memcpy(rw_image + proglen, temp, ilen);
	proglen += ilen;
	addrs[0] = proglen;
	prog = temp;

	for (i = 1; i <= insn_cnt; i++, insn++) {
		const s32 imm32 = insn->imm;
		u32 dst_reg = insn->dst_reg;
		u32 src_reg = insn->src_reg;
		u8 b2 = 0, b3 = 0;
		u8 *start_of_ldx;
		s64 jmp_offset;
		s16 insn_off;
		u8 jmp_cond;
		u8 *func;
		int nops;

		switch (insn->code) {
			 
		case BPF_ALU | BPF_ADD | BPF_X:
		case BPF_ALU | BPF_SUB | BPF_X:
		case BPF_ALU | BPF_AND | BPF_X:
		case BPF_ALU | BPF_OR | BPF_X:
		case BPF_ALU | BPF_XOR | BPF_X:
		case BPF_ALU64 | BPF_ADD | BPF_X:
		case BPF_ALU64 | BPF_SUB | BPF_X:
		case BPF_ALU64 | BPF_AND | BPF_X:
		case BPF_ALU64 | BPF_OR | BPF_X:
		case BPF_ALU64 | BPF_XOR | BPF_X:
			maybe_emit_mod(&prog, dst_reg, src_reg,
				       BPF_CLASS(insn->code) == BPF_ALU64);
			b2 = simple_alu_opcodes[BPF_OP(insn->code)];
			EMIT2(b2, add_2reg(0xC0, dst_reg, src_reg));
			break;

		case BPF_ALU64 | BPF_MOV | BPF_X:
		case BPF_ALU | BPF_MOV | BPF_X:
			if (insn->off == 0)
				emit_mov_reg(&prog,
					     BPF_CLASS(insn->code) == BPF_ALU64,
					     dst_reg, src_reg);
			else
				emit_movsx_reg(&prog, insn->off,
					       BPF_CLASS(insn->code) == BPF_ALU64,
					       dst_reg, src_reg);
			break;

			 
		case BPF_ALU | BPF_NEG:
		case BPF_ALU64 | BPF_NEG:
			maybe_emit_1mod(&prog, dst_reg,
					BPF_CLASS(insn->code) == BPF_ALU64);
			EMIT2(0xF7, add_1reg(0xD8, dst_reg));
			break;

		case BPF_ALU | BPF_ADD | BPF_K:
		case BPF_ALU | BPF_SUB | BPF_K:
		case BPF_ALU | BPF_AND | BPF_K:
		case BPF_ALU | BPF_OR | BPF_K:
		case BPF_ALU | BPF_XOR | BPF_K:
		case BPF_ALU64 | BPF_ADD | BPF_K:
		case BPF_ALU64 | BPF_SUB | BPF_K:
		case BPF_ALU64 | BPF_AND | BPF_K:
		case BPF_ALU64 | BPF_OR | BPF_K:
		case BPF_ALU64 | BPF_XOR | BPF_K:
			maybe_emit_1mod(&prog, dst_reg,
					BPF_CLASS(insn->code) == BPF_ALU64);

			 
			switch (BPF_OP(insn->code)) {
			case BPF_ADD:
				b3 = 0xC0;
				b2 = 0x05;
				break;
			case BPF_SUB:
				b3 = 0xE8;
				b2 = 0x2D;
				break;
			case BPF_AND:
				b3 = 0xE0;
				b2 = 0x25;
				break;
			case BPF_OR:
				b3 = 0xC8;
				b2 = 0x0D;
				break;
			case BPF_XOR:
				b3 = 0xF0;
				b2 = 0x35;
				break;
			}

			if (is_imm8(imm32))
				EMIT3(0x83, add_1reg(b3, dst_reg), imm32);
			else if (is_axreg(dst_reg))
				EMIT1_off32(b2, imm32);
			else
				EMIT2_off32(0x81, add_1reg(b3, dst_reg), imm32);
			break;

		case BPF_ALU64 | BPF_MOV | BPF_K:
		case BPF_ALU | BPF_MOV | BPF_K:
			emit_mov_imm32(&prog, BPF_CLASS(insn->code) == BPF_ALU64,
				       dst_reg, imm32);
			break;

		case BPF_LD | BPF_IMM | BPF_DW:
			emit_mov_imm64(&prog, dst_reg, insn[1].imm, insn[0].imm);
			insn++;
			i++;
			break;

			 
		case BPF_ALU | BPF_MOD | BPF_X:
		case BPF_ALU | BPF_DIV | BPF_X:
		case BPF_ALU | BPF_MOD | BPF_K:
		case BPF_ALU | BPF_DIV | BPF_K:
		case BPF_ALU64 | BPF_MOD | BPF_X:
		case BPF_ALU64 | BPF_DIV | BPF_X:
		case BPF_ALU64 | BPF_MOD | BPF_K:
		case BPF_ALU64 | BPF_DIV | BPF_K: {
			bool is64 = BPF_CLASS(insn->code) == BPF_ALU64;

			if (dst_reg != BPF_REG_0)
				EMIT1(0x50);  
			if (dst_reg != BPF_REG_3)
				EMIT1(0x52);  

			if (BPF_SRC(insn->code) == BPF_X) {
				if (src_reg == BPF_REG_0 ||
				    src_reg == BPF_REG_3) {
					 
					EMIT_mov(AUX_REG, src_reg);
					src_reg = AUX_REG;
				}
			} else {
				 
				EMIT3_off32(0x49, 0xC7, 0xC3, imm32);
				src_reg = AUX_REG;
			}

			if (dst_reg != BPF_REG_0)
				 
				emit_mov_reg(&prog, is64, BPF_REG_0, dst_reg);

			if (insn->off == 0) {
				 
				EMIT2(0x31, 0xd2);

				 
				maybe_emit_1mod(&prog, src_reg, is64);
				EMIT2(0xF7, add_1reg(0xF0, src_reg));
			} else {
				if (BPF_CLASS(insn->code) == BPF_ALU)
					EMIT1(0x99);  
				else
					EMIT2(0x48, 0x99);  

				 
				maybe_emit_1mod(&prog, src_reg, is64);
				EMIT2(0xF7, add_1reg(0xF8, src_reg));
			}

			if (BPF_OP(insn->code) == BPF_MOD &&
			    dst_reg != BPF_REG_3)
				 
				emit_mov_reg(&prog, is64, dst_reg, BPF_REG_3);
			else if (BPF_OP(insn->code) == BPF_DIV &&
				 dst_reg != BPF_REG_0)
				 
				emit_mov_reg(&prog, is64, dst_reg, BPF_REG_0);

			if (dst_reg != BPF_REG_3)
				EMIT1(0x5A);  
			if (dst_reg != BPF_REG_0)
				EMIT1(0x58);  
			break;
		}

		case BPF_ALU | BPF_MUL | BPF_K:
		case BPF_ALU64 | BPF_MUL | BPF_K:
			maybe_emit_mod(&prog, dst_reg, dst_reg,
				       BPF_CLASS(insn->code) == BPF_ALU64);

			if (is_imm8(imm32))
				 
				EMIT3(0x6B, add_2reg(0xC0, dst_reg, dst_reg),
				      imm32);
			else
				 
				EMIT2_off32(0x69,
					    add_2reg(0xC0, dst_reg, dst_reg),
					    imm32);
			break;

		case BPF_ALU | BPF_MUL | BPF_X:
		case BPF_ALU64 | BPF_MUL | BPF_X:
			maybe_emit_mod(&prog, src_reg, dst_reg,
				       BPF_CLASS(insn->code) == BPF_ALU64);

			 
			EMIT3(0x0F, 0xAF, add_2reg(0xC0, src_reg, dst_reg));
			break;

			 
		case BPF_ALU | BPF_LSH | BPF_K:
		case BPF_ALU | BPF_RSH | BPF_K:
		case BPF_ALU | BPF_ARSH | BPF_K:
		case BPF_ALU64 | BPF_LSH | BPF_K:
		case BPF_ALU64 | BPF_RSH | BPF_K:
		case BPF_ALU64 | BPF_ARSH | BPF_K:
			maybe_emit_1mod(&prog, dst_reg,
					BPF_CLASS(insn->code) == BPF_ALU64);

			b3 = simple_alu_opcodes[BPF_OP(insn->code)];
			if (imm32 == 1)
				EMIT2(0xD1, add_1reg(b3, dst_reg));
			else
				EMIT3(0xC1, add_1reg(b3, dst_reg), imm32);
			break;

		case BPF_ALU | BPF_LSH | BPF_X:
		case BPF_ALU | BPF_RSH | BPF_X:
		case BPF_ALU | BPF_ARSH | BPF_X:
		case BPF_ALU64 | BPF_LSH | BPF_X:
		case BPF_ALU64 | BPF_RSH | BPF_X:
		case BPF_ALU64 | BPF_ARSH | BPF_X:
			 
			if (boot_cpu_has(X86_FEATURE_BMI2) && src_reg != BPF_REG_4) {
				 
				bool w = (BPF_CLASS(insn->code) == BPF_ALU64);
				u8 op;

				switch (BPF_OP(insn->code)) {
				case BPF_LSH:
					op = 1;  
					break;
				case BPF_RSH:
					op = 3;  
					break;
				case BPF_ARSH:
					op = 2;  
					break;
				}

				emit_shiftx(&prog, dst_reg, src_reg, w, op);

				break;
			}

			if (src_reg != BPF_REG_4) {  
				 
				if (dst_reg == BPF_REG_4) {
					 
					EMIT_mov(AUX_REG, dst_reg);
					dst_reg = AUX_REG;
				} else {
					EMIT1(0x51);  
				}
				 
				EMIT_mov(BPF_REG_4, src_reg);
			}

			 
			maybe_emit_1mod(&prog, dst_reg,
					BPF_CLASS(insn->code) == BPF_ALU64);

			b3 = simple_alu_opcodes[BPF_OP(insn->code)];
			EMIT2(0xD3, add_1reg(b3, dst_reg));

			if (src_reg != BPF_REG_4) {
				if (insn->dst_reg == BPF_REG_4)
					 
					EMIT_mov(insn->dst_reg, AUX_REG);
				else
					EMIT1(0x59);  
			}

			break;

		case BPF_ALU | BPF_END | BPF_FROM_BE:
		case BPF_ALU64 | BPF_END | BPF_FROM_LE:
			switch (imm32) {
			case 16:
				 
				EMIT1(0x66);
				if (is_ereg(dst_reg))
					EMIT1(0x41);
				EMIT3(0xC1, add_1reg(0xC8, dst_reg), 8);

				 
				if (is_ereg(dst_reg))
					EMIT3(0x45, 0x0F, 0xB7);
				else
					EMIT2(0x0F, 0xB7);
				EMIT1(add_2reg(0xC0, dst_reg, dst_reg));
				break;
			case 32:
				 
				if (is_ereg(dst_reg))
					EMIT2(0x41, 0x0F);
				else
					EMIT1(0x0F);
				EMIT1(add_1reg(0xC8, dst_reg));
				break;
			case 64:
				 
				EMIT3(add_1mod(0x48, dst_reg), 0x0F,
				      add_1reg(0xC8, dst_reg));
				break;
			}
			break;

		case BPF_ALU | BPF_END | BPF_FROM_LE:
			switch (imm32) {
			case 16:
				 
				if (is_ereg(dst_reg))
					EMIT3(0x45, 0x0F, 0xB7);
				else
					EMIT2(0x0F, 0xB7);
				EMIT1(add_2reg(0xC0, dst_reg, dst_reg));
				break;
			case 32:
				 
				if (is_ereg(dst_reg))
					EMIT1(0x45);
				EMIT2(0x89, add_2reg(0xC0, dst_reg, dst_reg));
				break;
			case 64:
				 
				break;
			}
			break;

			 
		case BPF_ST | BPF_NOSPEC:
			EMIT_LFENCE();
			break;

			 
		case BPF_ST | BPF_MEM | BPF_B:
			if (is_ereg(dst_reg))
				EMIT2(0x41, 0xC6);
			else
				EMIT1(0xC6);
			goto st;
		case BPF_ST | BPF_MEM | BPF_H:
			if (is_ereg(dst_reg))
				EMIT3(0x66, 0x41, 0xC7);
			else
				EMIT2(0x66, 0xC7);
			goto st;
		case BPF_ST | BPF_MEM | BPF_W:
			if (is_ereg(dst_reg))
				EMIT2(0x41, 0xC7);
			else
				EMIT1(0xC7);
			goto st;
		case BPF_ST | BPF_MEM | BPF_DW:
			EMIT2(add_1mod(0x48, dst_reg), 0xC7);

st:			if (is_imm8(insn->off))
				EMIT2(add_1reg(0x40, dst_reg), insn->off);
			else
				EMIT1_off32(add_1reg(0x80, dst_reg), insn->off);

			EMIT(imm32, bpf_size_to_x86_bytes(BPF_SIZE(insn->code)));
			break;

			 
		case BPF_STX | BPF_MEM | BPF_B:
		case BPF_STX | BPF_MEM | BPF_H:
		case BPF_STX | BPF_MEM | BPF_W:
		case BPF_STX | BPF_MEM | BPF_DW:
			emit_stx(&prog, BPF_SIZE(insn->code), dst_reg, src_reg, insn->off);
			break;

			 
		case BPF_LDX | BPF_MEM | BPF_B:
		case BPF_LDX | BPF_PROBE_MEM | BPF_B:
		case BPF_LDX | BPF_MEM | BPF_H:
		case BPF_LDX | BPF_PROBE_MEM | BPF_H:
		case BPF_LDX | BPF_MEM | BPF_W:
		case BPF_LDX | BPF_PROBE_MEM | BPF_W:
		case BPF_LDX | BPF_MEM | BPF_DW:
		case BPF_LDX | BPF_PROBE_MEM | BPF_DW:
			 
		case BPF_LDX | BPF_MEMSX | BPF_B:
		case BPF_LDX | BPF_MEMSX | BPF_H:
		case BPF_LDX | BPF_MEMSX | BPF_W:
		case BPF_LDX | BPF_PROBE_MEMSX | BPF_B:
		case BPF_LDX | BPF_PROBE_MEMSX | BPF_H:
		case BPF_LDX | BPF_PROBE_MEMSX | BPF_W:
			insn_off = insn->off;

			if (BPF_MODE(insn->code) == BPF_PROBE_MEM ||
			    BPF_MODE(insn->code) == BPF_PROBE_MEMSX) {
				 

				u64 limit = TASK_SIZE_MAX + PAGE_SIZE;
				u8 *end_of_jmp;

				 
				insn_off = 0;

				 
				EMIT2(add_1mod(0x48, AUX_REG), add_1reg(0xB8, AUX_REG));
				EMIT((u32)limit, 4);
				EMIT(limit >> 32, 4);

				if (insn->off) {
					 
					maybe_emit_1mod(&prog, src_reg, true);
					EMIT2_off32(0x81, add_1reg(0xC0, src_reg), insn->off);
				}

				 
				maybe_emit_mod(&prog, src_reg, AUX_REG, true);
				EMIT2(0x39, add_2reg(0xC0, src_reg, AUX_REG));

				 
				EMIT2(X86_JAE, 0);
				end_of_jmp = prog;

				 
				emit_mov_imm32(&prog, false, dst_reg, 0);
				 
				EMIT2(0xEB, 0);

				 
				start_of_ldx = prog;
				end_of_jmp[-1] = start_of_ldx - end_of_jmp;
			}
			if (BPF_MODE(insn->code) == BPF_PROBE_MEMSX ||
			    BPF_MODE(insn->code) == BPF_MEMSX)
				emit_ldsx(&prog, BPF_SIZE(insn->code), dst_reg, src_reg, insn_off);
			else
				emit_ldx(&prog, BPF_SIZE(insn->code), dst_reg, src_reg, insn_off);
			if (BPF_MODE(insn->code) == BPF_PROBE_MEM ||
			    BPF_MODE(insn->code) == BPF_PROBE_MEMSX) {
				struct exception_table_entry *ex;
				u8 *_insn = image + proglen + (start_of_ldx - temp);
				s64 delta;

				 
				start_of_ldx[-1] = prog - start_of_ldx;

				if (insn->off && src_reg != dst_reg) {
					 
					maybe_emit_1mod(&prog, src_reg, true);
					EMIT2_off32(0x81, add_1reg(0xE8, src_reg), insn->off);
				}

				if (!bpf_prog->aux->extable)
					break;

				if (excnt >= bpf_prog->aux->num_exentries) {
					pr_err("ex gen bug\n");
					return -EFAULT;
				}
				ex = &bpf_prog->aux->extable[excnt++];

				delta = _insn - (u8 *)&ex->insn;
				if (!is_simm32(delta)) {
					pr_err("extable->insn doesn't fit into 32-bit\n");
					return -EFAULT;
				}
				 
				ex = (void *)rw_image + ((void *)ex - (void *)image);

				ex->insn = delta;

				ex->data = EX_TYPE_BPF;

				if (dst_reg > BPF_REG_9) {
					pr_err("verifier error\n");
					return -EFAULT;
				}
				 
				ex->fixup = (prog - start_of_ldx) | (reg2pt_regs[dst_reg] << 8);
			}
			break;

		case BPF_STX | BPF_ATOMIC | BPF_W:
		case BPF_STX | BPF_ATOMIC | BPF_DW:
			if (insn->imm == (BPF_AND | BPF_FETCH) ||
			    insn->imm == (BPF_OR | BPF_FETCH) ||
			    insn->imm == (BPF_XOR | BPF_FETCH)) {
				bool is64 = BPF_SIZE(insn->code) == BPF_DW;
				u32 real_src_reg = src_reg;
				u32 real_dst_reg = dst_reg;
				u8 *branch_target;

				 

				 
				emit_mov_reg(&prog, true, BPF_REG_AX, BPF_REG_0);
				if (src_reg == BPF_REG_0)
					real_src_reg = BPF_REG_AX;
				if (dst_reg == BPF_REG_0)
					real_dst_reg = BPF_REG_AX;

				branch_target = prog;
				 
				emit_ldx(&prog, BPF_SIZE(insn->code),
					 BPF_REG_0, real_dst_reg, insn->off);
				 
				emit_mov_reg(&prog, is64, AUX_REG, BPF_REG_0);
				maybe_emit_mod(&prog, AUX_REG, real_src_reg, is64);
				EMIT2(simple_alu_opcodes[BPF_OP(insn->imm)],
				      add_2reg(0xC0, AUX_REG, real_src_reg));
				 
				err = emit_atomic(&prog, BPF_CMPXCHG,
						  real_dst_reg, AUX_REG,
						  insn->off,
						  BPF_SIZE(insn->code));
				if (WARN_ON(err))
					return err;
				 
				EMIT2(X86_JNE, -(prog - branch_target) - 2);
				 
				emit_mov_reg(&prog, is64, real_src_reg, BPF_REG_0);
				 
				emit_mov_reg(&prog, true, BPF_REG_0, BPF_REG_AX);
				break;
			}

			err = emit_atomic(&prog, insn->imm, dst_reg, src_reg,
					  insn->off, BPF_SIZE(insn->code));
			if (err)
				return err;
			break;

			 
		case BPF_JMP | BPF_CALL: {
			int offs;

			func = (u8 *) __bpf_call_base + imm32;
			if (tail_call_reachable) {
				RESTORE_TAIL_CALL_CNT(bpf_prog->aux->stack_depth);
				if (!imm32)
					return -EINVAL;
				offs = 7 + x86_call_depth_emit_accounting(&prog, func);
			} else {
				if (!imm32)
					return -EINVAL;
				offs = x86_call_depth_emit_accounting(&prog, func);
			}
			if (emit_call(&prog, func, image + addrs[i - 1] + offs))
				return -EINVAL;
			break;
		}

		case BPF_JMP | BPF_TAIL_CALL:
			if (imm32)
				emit_bpf_tail_call_direct(&bpf_prog->aux->poke_tab[imm32 - 1],
							  &prog, image + addrs[i - 1],
							  callee_regs_used,
							  bpf_prog->aux->stack_depth,
							  ctx);
			else
				emit_bpf_tail_call_indirect(&prog,
							    callee_regs_used,
							    bpf_prog->aux->stack_depth,
							    image + addrs[i - 1],
							    ctx);
			break;

			 
		case BPF_JMP | BPF_JEQ | BPF_X:
		case BPF_JMP | BPF_JNE | BPF_X:
		case BPF_JMP | BPF_JGT | BPF_X:
		case BPF_JMP | BPF_JLT | BPF_X:
		case BPF_JMP | BPF_JGE | BPF_X:
		case BPF_JMP | BPF_JLE | BPF_X:
		case BPF_JMP | BPF_JSGT | BPF_X:
		case BPF_JMP | BPF_JSLT | BPF_X:
		case BPF_JMP | BPF_JSGE | BPF_X:
		case BPF_JMP | BPF_JSLE | BPF_X:
		case BPF_JMP32 | BPF_JEQ | BPF_X:
		case BPF_JMP32 | BPF_JNE | BPF_X:
		case BPF_JMP32 | BPF_JGT | BPF_X:
		case BPF_JMP32 | BPF_JLT | BPF_X:
		case BPF_JMP32 | BPF_JGE | BPF_X:
		case BPF_JMP32 | BPF_JLE | BPF_X:
		case BPF_JMP32 | BPF_JSGT | BPF_X:
		case BPF_JMP32 | BPF_JSLT | BPF_X:
		case BPF_JMP32 | BPF_JSGE | BPF_X:
		case BPF_JMP32 | BPF_JSLE | BPF_X:
			 
			maybe_emit_mod(&prog, dst_reg, src_reg,
				       BPF_CLASS(insn->code) == BPF_JMP);
			EMIT2(0x39, add_2reg(0xC0, dst_reg, src_reg));
			goto emit_cond_jmp;

		case BPF_JMP | BPF_JSET | BPF_X:
		case BPF_JMP32 | BPF_JSET | BPF_X:
			 
			maybe_emit_mod(&prog, dst_reg, src_reg,
				       BPF_CLASS(insn->code) == BPF_JMP);
			EMIT2(0x85, add_2reg(0xC0, dst_reg, src_reg));
			goto emit_cond_jmp;

		case BPF_JMP | BPF_JSET | BPF_K:
		case BPF_JMP32 | BPF_JSET | BPF_K:
			 
			maybe_emit_1mod(&prog, dst_reg,
					BPF_CLASS(insn->code) == BPF_JMP);
			EMIT2_off32(0xF7, add_1reg(0xC0, dst_reg), imm32);
			goto emit_cond_jmp;

		case BPF_JMP | BPF_JEQ | BPF_K:
		case BPF_JMP | BPF_JNE | BPF_K:
		case BPF_JMP | BPF_JGT | BPF_K:
		case BPF_JMP | BPF_JLT | BPF_K:
		case BPF_JMP | BPF_JGE | BPF_K:
		case BPF_JMP | BPF_JLE | BPF_K:
		case BPF_JMP | BPF_JSGT | BPF_K:
		case BPF_JMP | BPF_JSLT | BPF_K:
		case BPF_JMP | BPF_JSGE | BPF_K:
		case BPF_JMP | BPF_JSLE | BPF_K:
		case BPF_JMP32 | BPF_JEQ | BPF_K:
		case BPF_JMP32 | BPF_JNE | BPF_K:
		case BPF_JMP32 | BPF_JGT | BPF_K:
		case BPF_JMP32 | BPF_JLT | BPF_K:
		case BPF_JMP32 | BPF_JGE | BPF_K:
		case BPF_JMP32 | BPF_JLE | BPF_K:
		case BPF_JMP32 | BPF_JSGT | BPF_K:
		case BPF_JMP32 | BPF_JSLT | BPF_K:
		case BPF_JMP32 | BPF_JSGE | BPF_K:
		case BPF_JMP32 | BPF_JSLE | BPF_K:
			 
			if (imm32 == 0) {
				maybe_emit_mod(&prog, dst_reg, dst_reg,
					       BPF_CLASS(insn->code) == BPF_JMP);
				EMIT2(0x85, add_2reg(0xC0, dst_reg, dst_reg));
				goto emit_cond_jmp;
			}

			 
			maybe_emit_1mod(&prog, dst_reg,
					BPF_CLASS(insn->code) == BPF_JMP);

			if (is_imm8(imm32))
				EMIT3(0x83, add_1reg(0xF8, dst_reg), imm32);
			else
				EMIT2_off32(0x81, add_1reg(0xF8, dst_reg), imm32);

emit_cond_jmp:		 
			switch (BPF_OP(insn->code)) {
			case BPF_JEQ:
				jmp_cond = X86_JE;
				break;
			case BPF_JSET:
			case BPF_JNE:
				jmp_cond = X86_JNE;
				break;
			case BPF_JGT:
				 
				jmp_cond = X86_JA;
				break;
			case BPF_JLT:
				 
				jmp_cond = X86_JB;
				break;
			case BPF_JGE:
				 
				jmp_cond = X86_JAE;
				break;
			case BPF_JLE:
				 
				jmp_cond = X86_JBE;
				break;
			case BPF_JSGT:
				 
				jmp_cond = X86_JG;
				break;
			case BPF_JSLT:
				 
				jmp_cond = X86_JL;
				break;
			case BPF_JSGE:
				 
				jmp_cond = X86_JGE;
				break;
			case BPF_JSLE:
				 
				jmp_cond = X86_JLE;
				break;
			default:  
				return -EFAULT;
			}
			jmp_offset = addrs[i + insn->off] - addrs[i];
			if (is_imm8(jmp_offset)) {
				if (jmp_padding) {
					 
					nops = INSN_SZ_DIFF - 2;
					if (nops != 0 && nops != 4) {
						pr_err("unexpected jmp_cond padding: %d bytes\n",
						       nops);
						return -EFAULT;
					}
					emit_nops(&prog, nops);
				}
				EMIT2(jmp_cond, jmp_offset);
			} else if (is_simm32(jmp_offset)) {
				EMIT2_off32(0x0F, jmp_cond + 0x10, jmp_offset);
			} else {
				pr_err("cond_jmp gen bug %llx\n", jmp_offset);
				return -EFAULT;
			}

			break;

		case BPF_JMP | BPF_JA:
		case BPF_JMP32 | BPF_JA:
			if (BPF_CLASS(insn->code) == BPF_JMP) {
				if (insn->off == -1)
					 
					jmp_offset = -2;
				else
					jmp_offset = addrs[i + insn->off] - addrs[i];
			} else {
				if (insn->imm == -1)
					jmp_offset = -2;
				else
					jmp_offset = addrs[i + insn->imm] - addrs[i];
			}

			if (!jmp_offset) {
				 
				if (jmp_padding) {
					 
					nops = INSN_SZ_DIFF;
					if (nops != 0 && nops != 2 && nops != 5) {
						pr_err("unexpected nop jump padding: %d bytes\n",
						       nops);
						return -EFAULT;
					}
					emit_nops(&prog, nops);
				}
				break;
			}
emit_jmp:
			if (is_imm8(jmp_offset)) {
				if (jmp_padding) {
					 
					nops = INSN_SZ_DIFF - 2;
					if (nops != 0 && nops != 3) {
						pr_err("unexpected jump padding: %d bytes\n",
						       nops);
						return -EFAULT;
					}
					emit_nops(&prog, INSN_SZ_DIFF - 2);
				}
				EMIT2(0xEB, jmp_offset);
			} else if (is_simm32(jmp_offset)) {
				EMIT1_off32(0xE9, jmp_offset);
			} else {
				pr_err("jmp gen bug %llx\n", jmp_offset);
				return -EFAULT;
			}
			break;

		case BPF_JMP | BPF_EXIT:
			if (seen_exit) {
				jmp_offset = ctx->cleanup_addr - addrs[i];
				goto emit_jmp;
			}
			seen_exit = true;
			 
			ctx->cleanup_addr = proglen;
			pop_callee_regs(&prog, callee_regs_used);
			EMIT1(0xC9);          
			emit_return(&prog, image + addrs[i - 1] + (prog - temp));
			break;

		default:
			 
			pr_err("bpf_jit: unknown opcode %02x\n", insn->code);
			return -EINVAL;
		}

		ilen = prog - temp;
		if (ilen > BPF_MAX_INSN_SIZE) {
			pr_err("bpf_jit: fatal insn size error\n");
			return -EFAULT;
		}

		if (image) {
			 
			if (unlikely(proglen + ilen > oldproglen ||
				     proglen + ilen != addrs[i])) {
				pr_err("bpf_jit: fatal error\n");
				return -EFAULT;
			}
			memcpy(rw_image + proglen, temp, ilen);
		}
		proglen += ilen;
		addrs[i] = proglen;
		prog = temp;
	}

	if (image && excnt != bpf_prog->aux->num_exentries) {
		pr_err("extable is not populated\n");
		return -EFAULT;
	}
	return proglen;
}

static void clean_stack_garbage(const struct btf_func_model *m,
				u8 **pprog, int nr_stack_slots,
				int stack_size)
{
	int arg_size, off;
	u8 *prog;

	 
	if (nr_stack_slots != 1)
		return;

	 
	arg_size = m->arg_size[m->nr_args - 1];
	if (arg_size <= 4) {
		off = -(stack_size - 4);
		prog = *pprog;
		 
		if (!is_imm8(off))
			EMIT2_off32(0xC7, 0x85, off);
		else
			EMIT3(0xC7, 0x45, off);
		EMIT(0, 4);
		*pprog = prog;
	}
}

 
static int get_nr_used_regs(const struct btf_func_model *m)
{
	int i, arg_regs, nr_used_regs = 0;

	for (i = 0; i < min_t(int, m->nr_args, MAX_BPF_FUNC_ARGS); i++) {
		arg_regs = (m->arg_size[i] + 7) / 8;
		if (nr_used_regs + arg_regs <= 6)
			nr_used_regs += arg_regs;

		if (nr_used_regs >= 6)
			break;
	}

	return nr_used_regs;
}

static void save_args(const struct btf_func_model *m, u8 **prog,
		      int stack_size, bool for_call_origin)
{
	int arg_regs, first_off = 0, nr_regs = 0, nr_stack_slots = 0;
	int i, j;

	 
	for (i = 0; i < min_t(int, m->nr_args, MAX_BPF_FUNC_ARGS); i++) {
		arg_regs = (m->arg_size[i] + 7) / 8;

		 
		if (nr_regs + arg_regs > 6) {
			 
			for (j = 0; j < arg_regs; j++) {
				emit_ldx(prog, BPF_DW, BPF_REG_0, BPF_REG_FP,
					 nr_stack_slots * 8 + 0x18);
				emit_stx(prog, BPF_DW, BPF_REG_FP, BPF_REG_0,
					 -stack_size);

				if (!nr_stack_slots)
					first_off = stack_size;
				stack_size -= 8;
				nr_stack_slots++;
			}
		} else {
			 
			if (for_call_origin) {
				nr_regs += arg_regs;
				continue;
			}

			 
			for (j = 0; j < arg_regs; j++) {
				emit_stx(prog, BPF_DW, BPF_REG_FP,
					 nr_regs == 5 ? X86_REG_R9 : BPF_REG_1 + nr_regs,
					 -stack_size);
				stack_size -= 8;
				nr_regs++;
			}
		}
	}

	clean_stack_garbage(m, prog, nr_stack_slots, first_off);
}

static void restore_regs(const struct btf_func_model *m, u8 **prog,
			 int stack_size)
{
	int i, j, arg_regs, nr_regs = 0;

	 
	for (i = 0; i < min_t(int, m->nr_args, MAX_BPF_FUNC_ARGS); i++) {
		arg_regs = (m->arg_size[i] + 7) / 8;
		if (nr_regs + arg_regs <= 6) {
			for (j = 0; j < arg_regs; j++) {
				emit_ldx(prog, BPF_DW,
					 nr_regs == 5 ? X86_REG_R9 : BPF_REG_1 + nr_regs,
					 BPF_REG_FP,
					 -stack_size);
				stack_size -= 8;
				nr_regs++;
			}
		} else {
			stack_size -= 8 * arg_regs;
		}

		if (nr_regs >= 6)
			break;
	}
}

static int invoke_bpf_prog(const struct btf_func_model *m, u8 **pprog,
			   struct bpf_tramp_link *l, int stack_size,
			   int run_ctx_off, bool save_ret)
{
	u8 *prog = *pprog;
	u8 *jmp_insn;
	int ctx_cookie_off = offsetof(struct bpf_tramp_run_ctx, bpf_cookie);
	struct bpf_prog *p = l->link.prog;
	u64 cookie = l->cookie;

	 
	emit_mov_imm64(&prog, BPF_REG_1, (long) cookie >> 32, (u32) (long) cookie);

	 
	emit_stx(&prog, BPF_DW, BPF_REG_FP, BPF_REG_1, -run_ctx_off + ctx_cookie_off);

	 
	emit_mov_imm64(&prog, BPF_REG_1, (long) p >> 32, (u32) (long) p);
	 
	if (!is_imm8(-run_ctx_off))
		EMIT3_off32(0x48, 0x8D, 0xB5, -run_ctx_off);
	else
		EMIT4(0x48, 0x8D, 0x75, -run_ctx_off);

	if (emit_rsb_call(&prog, bpf_trampoline_enter(p), prog))
		return -EINVAL;
	 
	emit_mov_reg(&prog, true, BPF_REG_6, BPF_REG_0);

	 
	EMIT3(0x48, 0x85, 0xC0);   
	 
	jmp_insn = prog;
	emit_nops(&prog, 2);

	 
	if (!is_imm8(-stack_size))
		EMIT3_off32(0x48, 0x8D, 0xBD, -stack_size);
	else
		EMIT4(0x48, 0x8D, 0x7D, -stack_size);
	 
	if (!p->jited)
		emit_mov_imm64(&prog, BPF_REG_2,
			       (long) p->insnsi >> 32,
			       (u32) (long) p->insnsi);
	 
	if (emit_rsb_call(&prog, p->bpf_func, prog))
		return -EINVAL;

	 
	if (save_ret)
		emit_stx(&prog, BPF_DW, BPF_REG_FP, BPF_REG_0, -8);

	 
	jmp_insn[0] = X86_JE;
	jmp_insn[1] = prog - jmp_insn - 2;

	 
	emit_mov_imm64(&prog, BPF_REG_1, (long) p >> 32, (u32) (long) p);
	 
	emit_mov_reg(&prog, true, BPF_REG_2, BPF_REG_6);
	 
	if (!is_imm8(-run_ctx_off))
		EMIT3_off32(0x48, 0x8D, 0x95, -run_ctx_off);
	else
		EMIT4(0x48, 0x8D, 0x55, -run_ctx_off);
	if (emit_rsb_call(&prog, bpf_trampoline_exit(p), prog))
		return -EINVAL;

	*pprog = prog;
	return 0;
}

static void emit_align(u8 **pprog, u32 align)
{
	u8 *target, *prog = *pprog;

	target = PTR_ALIGN(prog, align);
	if (target != prog)
		emit_nops(&prog, target - prog);

	*pprog = prog;
}

static int emit_cond_near_jump(u8 **pprog, void *func, void *ip, u8 jmp_cond)
{
	u8 *prog = *pprog;
	s64 offset;

	offset = func - (ip + 2 + 4);
	if (!is_simm32(offset)) {
		pr_err("Target %p is out of range\n", func);
		return -EINVAL;
	}
	EMIT2_off32(0x0F, jmp_cond + 0x10, offset);
	*pprog = prog;
	return 0;
}

static int invoke_bpf(const struct btf_func_model *m, u8 **pprog,
		      struct bpf_tramp_links *tl, int stack_size,
		      int run_ctx_off, bool save_ret)
{
	int i;
	u8 *prog = *pprog;

	for (i = 0; i < tl->nr_links; i++) {
		if (invoke_bpf_prog(m, &prog, tl->links[i], stack_size,
				    run_ctx_off, save_ret))
			return -EINVAL;
	}
	*pprog = prog;
	return 0;
}

static int invoke_bpf_mod_ret(const struct btf_func_model *m, u8 **pprog,
			      struct bpf_tramp_links *tl, int stack_size,
			      int run_ctx_off, u8 **branches)
{
	u8 *prog = *pprog;
	int i;

	 
	emit_mov_imm32(&prog, false, BPF_REG_0, 0);
	emit_stx(&prog, BPF_DW, BPF_REG_FP, BPF_REG_0, -8);
	for (i = 0; i < tl->nr_links; i++) {
		if (invoke_bpf_prog(m, &prog, tl->links[i], stack_size, run_ctx_off, true))
			return -EINVAL;

		 
		 
		EMIT4(0x48, 0x83, 0x7d, 0xf8); EMIT1(0x00);

		 
		branches[i] = prog;
		emit_nops(&prog, 4 + 2);
	}

	*pprog = prog;
	return 0;
}

 
int arch_prepare_bpf_trampoline(struct bpf_tramp_image *im, void *image, void *image_end,
				const struct btf_func_model *m, u32 flags,
				struct bpf_tramp_links *tlinks,
				void *func_addr)
{
	int i, ret, nr_regs = m->nr_args, stack_size = 0;
	int regs_off, nregs_off, ip_off, run_ctx_off, arg_stack_off, rbx_off;
	struct bpf_tramp_links *fentry = &tlinks[BPF_TRAMP_FENTRY];
	struct bpf_tramp_links *fexit = &tlinks[BPF_TRAMP_FEXIT];
	struct bpf_tramp_links *fmod_ret = &tlinks[BPF_TRAMP_MODIFY_RETURN];
	void *orig_call = func_addr;
	u8 **branches = NULL;
	u8 *prog;
	bool save_ret;

	 
	for (i = 0; i < m->nr_args; i++)
		if (m->arg_flags[i] & BTF_FMODEL_STRUCT_ARG)
			nr_regs += (m->arg_size[i] + 7) / 8 - 1;

	 
	if (nr_regs > MAX_BPF_FUNC_ARGS)
		return -ENOTSUPP;

	 

	 
	save_ret = flags & (BPF_TRAMP_F_CALL_ORIG | BPF_TRAMP_F_RET_FENTRY_RET);
	if (save_ret)
		stack_size += 8;

	stack_size += nr_regs * 8;
	regs_off = stack_size;

	 
	stack_size += 8;
	nregs_off = stack_size;

	if (flags & BPF_TRAMP_F_IP_ARG)
		stack_size += 8;  

	ip_off = stack_size;

	stack_size += 8;
	rbx_off = stack_size;

	stack_size += (sizeof(struct bpf_tramp_run_ctx) + 7) & ~0x7;
	run_ctx_off = stack_size;

	if (nr_regs > 6 && (flags & BPF_TRAMP_F_CALL_ORIG)) {
		 
		stack_size += (nr_regs - get_nr_used_regs(m)) * 8;
		 
		stack_size += (stack_size % 16) ? 0 : 8;
	}

	arg_stack_off = stack_size;

	if (flags & BPF_TRAMP_F_SKIP_FRAME) {
		 
		if (is_endbr(*(u32 *)orig_call))
			orig_call += ENDBR_INSN_SIZE;
		orig_call += X86_PATCH_SIZE;
	}

	prog = image;

	EMIT_ENDBR();
	 
	x86_call_depth_emit_accounting(&prog, NULL);
	EMIT1(0x55);		  
	EMIT3(0x48, 0x89, 0xE5);  
	if (!is_imm8(stack_size))
		 
		EMIT3_off32(0x48, 0x81, 0xEC, stack_size);
	else
		 
		EMIT4(0x48, 0x83, 0xEC, stack_size);
	if (flags & BPF_TRAMP_F_TAIL_CALL_CTX)
		EMIT1(0x50);		 
	 
	emit_stx(&prog, BPF_DW, BPF_REG_FP, BPF_REG_6, -rbx_off);

	 
	emit_mov_imm64(&prog, BPF_REG_0, 0, (u32) nr_regs);
	emit_stx(&prog, BPF_DW, BPF_REG_FP, BPF_REG_0, -nregs_off);

	if (flags & BPF_TRAMP_F_IP_ARG) {
		 
		emit_mov_imm64(&prog, BPF_REG_0, (long) func_addr >> 32, (u32) (long) func_addr);
		emit_stx(&prog, BPF_DW, BPF_REG_FP, BPF_REG_0, -ip_off);
	}

	save_args(m, &prog, regs_off, false);

	if (flags & BPF_TRAMP_F_CALL_ORIG) {
		 
		emit_mov_imm64(&prog, BPF_REG_1, (long) im >> 32, (u32) (long) im);
		if (emit_rsb_call(&prog, __bpf_tramp_enter, prog)) {
			ret = -EINVAL;
			goto cleanup;
		}
	}

	if (fentry->nr_links)
		if (invoke_bpf(m, &prog, fentry, regs_off, run_ctx_off,
			       flags & BPF_TRAMP_F_RET_FENTRY_RET))
			return -EINVAL;

	if (fmod_ret->nr_links) {
		branches = kcalloc(fmod_ret->nr_links, sizeof(u8 *),
				   GFP_KERNEL);
		if (!branches)
			return -ENOMEM;

		if (invoke_bpf_mod_ret(m, &prog, fmod_ret, regs_off,
				       run_ctx_off, branches)) {
			ret = -EINVAL;
			goto cleanup;
		}
	}

	if (flags & BPF_TRAMP_F_CALL_ORIG) {
		restore_regs(m, &prog, regs_off);
		save_args(m, &prog, arg_stack_off, true);

		if (flags & BPF_TRAMP_F_TAIL_CALL_CTX)
			 
			RESTORE_TAIL_CALL_CNT(stack_size);

		if (flags & BPF_TRAMP_F_ORIG_STACK) {
			emit_ldx(&prog, BPF_DW, BPF_REG_6, BPF_REG_FP, 8);
			EMIT2(0xff, 0xd3);  
		} else {
			 
			if (emit_rsb_call(&prog, orig_call, prog)) {
				ret = -EINVAL;
				goto cleanup;
			}
		}
		 
		emit_stx(&prog, BPF_DW, BPF_REG_FP, BPF_REG_0, -8);
		im->ip_after_call = prog;
		memcpy(prog, x86_nops[5], X86_PATCH_SIZE);
		prog += X86_PATCH_SIZE;
	}

	if (fmod_ret->nr_links) {
		 
		emit_align(&prog, 16);
		 
		for (i = 0; i < fmod_ret->nr_links; i++)
			emit_cond_near_jump(&branches[i], prog, branches[i],
					    X86_JNE);
	}

	if (fexit->nr_links)
		if (invoke_bpf(m, &prog, fexit, regs_off, run_ctx_off, false)) {
			ret = -EINVAL;
			goto cleanup;
		}

	if (flags & BPF_TRAMP_F_RESTORE_REGS)
		restore_regs(m, &prog, regs_off);

	 
	if (flags & BPF_TRAMP_F_CALL_ORIG) {
		im->ip_epilogue = prog;
		 
		emit_mov_imm64(&prog, BPF_REG_1, (long) im >> 32, (u32) (long) im);
		if (emit_rsb_call(&prog, __bpf_tramp_exit, prog)) {
			ret = -EINVAL;
			goto cleanup;
		}
	} else if (flags & BPF_TRAMP_F_TAIL_CALL_CTX)
		 
		RESTORE_TAIL_CALL_CNT(stack_size);

	 
	if (save_ret)
		emit_ldx(&prog, BPF_DW, BPF_REG_0, BPF_REG_FP, -8);

	emit_ldx(&prog, BPF_DW, BPF_REG_6, BPF_REG_FP, -rbx_off);
	EMIT1(0xC9);  
	if (flags & BPF_TRAMP_F_SKIP_FRAME)
		 
		EMIT4(0x48, 0x83, 0xC4, 8);  
	emit_return(&prog, prog);
	 
	if (WARN_ON_ONCE(prog > (u8 *)image_end - BPF_INSN_SAFETY)) {
		ret = -EFAULT;
		goto cleanup;
	}
	ret = prog - (u8 *)image;

cleanup:
	kfree(branches);
	return ret;
}

static int emit_bpf_dispatcher(u8 **pprog, int a, int b, s64 *progs, u8 *image, u8 *buf)
{
	u8 *jg_reloc, *prog = *pprog;
	int pivot, err, jg_bytes = 1;
	s64 jg_offset;

	if (a == b) {
		 
		EMIT1(add_1mod(0x48, BPF_REG_3));	 
		if (!is_simm32(progs[a]))
			return -1;
		EMIT2_off32(0x81, add_1reg(0xF8, BPF_REG_3),
			    progs[a]);
		err = emit_cond_near_jump(&prog,	 
					  (void *)progs[a], image + (prog - buf),
					  X86_JE);
		if (err)
			return err;

		emit_indirect_jump(&prog, 2  , image + (prog - buf));

		*pprog = prog;
		return 0;
	}

	 
	pivot = (b - a) / 2;
	EMIT1(add_1mod(0x48, BPF_REG_3));		 
	if (!is_simm32(progs[a + pivot]))
		return -1;
	EMIT2_off32(0x81, add_1reg(0xF8, BPF_REG_3), progs[a + pivot]);

	if (pivot > 2) {				 
		 
		jg_bytes = 4;
		EMIT2_off32(0x0F, X86_JG + 0x10, 0);
	} else {
		EMIT2(X86_JG, 0);
	}
	jg_reloc = prog;

	err = emit_bpf_dispatcher(&prog, a, a + pivot,	 
				  progs, image, buf);
	if (err)
		return err;

	 
	emit_align(&prog, 16);
	jg_offset = prog - jg_reloc;
	emit_code(jg_reloc - jg_bytes, jg_offset, jg_bytes);

	err = emit_bpf_dispatcher(&prog, a + pivot + 1,	 
				  b, progs, image, buf);
	if (err)
		return err;

	*pprog = prog;
	return 0;
}

static int cmp_ips(const void *a, const void *b)
{
	const s64 *ipa = a;
	const s64 *ipb = b;

	if (*ipa > *ipb)
		return 1;
	if (*ipa < *ipb)
		return -1;
	return 0;
}

int arch_prepare_bpf_dispatcher(void *image, void *buf, s64 *funcs, int num_funcs)
{
	u8 *prog = buf;

	sort(funcs, num_funcs, sizeof(funcs[0]), cmp_ips, NULL);
	return emit_bpf_dispatcher(&prog, 0, num_funcs - 1, funcs, image, buf);
}

struct x64_jit_data {
	struct bpf_binary_header *rw_header;
	struct bpf_binary_header *header;
	int *addrs;
	u8 *image;
	int proglen;
	struct jit_context ctx;
};

#define MAX_PASSES 20
#define PADDING_PASSES (MAX_PASSES - 5)

struct bpf_prog *bpf_int_jit_compile(struct bpf_prog *prog)
{
	struct bpf_binary_header *rw_header = NULL;
	struct bpf_binary_header *header = NULL;
	struct bpf_prog *tmp, *orig_prog = prog;
	struct x64_jit_data *jit_data;
	int proglen, oldproglen = 0;
	struct jit_context ctx = {};
	bool tmp_blinded = false;
	bool extra_pass = false;
	bool padding = false;
	u8 *rw_image = NULL;
	u8 *image = NULL;
	int *addrs;
	int pass;
	int i;

	if (!prog->jit_requested)
		return orig_prog;

	tmp = bpf_jit_blind_constants(prog);
	 
	if (IS_ERR(tmp))
		return orig_prog;
	if (tmp != prog) {
		tmp_blinded = true;
		prog = tmp;
	}

	jit_data = prog->aux->jit_data;
	if (!jit_data) {
		jit_data = kzalloc(sizeof(*jit_data), GFP_KERNEL);
		if (!jit_data) {
			prog = orig_prog;
			goto out;
		}
		prog->aux->jit_data = jit_data;
	}
	addrs = jit_data->addrs;
	if (addrs) {
		ctx = jit_data->ctx;
		oldproglen = jit_data->proglen;
		image = jit_data->image;
		header = jit_data->header;
		rw_header = jit_data->rw_header;
		rw_image = (void *)rw_header + ((void *)image - (void *)header);
		extra_pass = true;
		padding = true;
		goto skip_init_addrs;
	}
	addrs = kvmalloc_array(prog->len + 1, sizeof(*addrs), GFP_KERNEL);
	if (!addrs) {
		prog = orig_prog;
		goto out_addrs;
	}

	 
	for (proglen = 0, i = 0; i <= prog->len; i++) {
		proglen += 64;
		addrs[i] = proglen;
	}
	ctx.cleanup_addr = proglen;
skip_init_addrs:

	 
	for (pass = 0; pass < MAX_PASSES || image; pass++) {
		if (!padding && pass >= PADDING_PASSES)
			padding = true;
		proglen = do_jit(prog, addrs, image, rw_image, oldproglen, &ctx, padding);
		if (proglen <= 0) {
out_image:
			image = NULL;
			if (header) {
				bpf_arch_text_copy(&header->size, &rw_header->size,
						   sizeof(rw_header->size));
				bpf_jit_binary_pack_free(header, rw_header);
			}
			 
			prog = orig_prog;
			if (extra_pass) {
				prog->bpf_func = NULL;
				prog->jited = 0;
				prog->jited_len = 0;
			}
			goto out_addrs;
		}
		if (image) {
			if (proglen != oldproglen) {
				pr_err("bpf_jit: proglen=%d != oldproglen=%d\n",
				       proglen, oldproglen);
				goto out_image;
			}
			break;
		}
		if (proglen == oldproglen) {
			 
			u32 align = __alignof__(struct exception_table_entry);
			u32 extable_size = prog->aux->num_exentries *
				sizeof(struct exception_table_entry);

			 
			header = bpf_jit_binary_pack_alloc(roundup(proglen, align) + extable_size,
							   &image, align, &rw_header, &rw_image,
							   jit_fill_hole);
			if (!header) {
				prog = orig_prog;
				goto out_addrs;
			}
			prog->aux->extable = (void *) image + roundup(proglen, align);
		}
		oldproglen = proglen;
		cond_resched();
	}

	if (bpf_jit_enable > 1)
		bpf_jit_dump(prog->len, proglen, pass + 1, rw_image);

	if (image) {
		if (!prog->is_func || extra_pass) {
			 
			if (WARN_ON(bpf_jit_binary_pack_finalize(prog, header, rw_header))) {
				 
				header = NULL;
				goto out_image;
			}

			bpf_tail_call_direct_fixup(prog);
		} else {
			jit_data->addrs = addrs;
			jit_data->ctx = ctx;
			jit_data->proglen = proglen;
			jit_data->image = image;
			jit_data->header = header;
			jit_data->rw_header = rw_header;
		}
		prog->bpf_func = (void *)image;
		prog->jited = 1;
		prog->jited_len = proglen;
	} else {
		prog = orig_prog;
	}

	if (!image || !prog->is_func || extra_pass) {
		if (image)
			bpf_prog_fill_jited_linfo(prog, addrs + 1);
out_addrs:
		kvfree(addrs);
		kfree(jit_data);
		prog->aux->jit_data = NULL;
	}
out:
	if (tmp_blinded)
		bpf_jit_prog_release_other(prog, prog == orig_prog ?
					   tmp : orig_prog);
	return prog;
}

bool bpf_jit_supports_kfunc_call(void)
{
	return true;
}

void *bpf_arch_text_copy(void *dst, void *src, size_t len)
{
	if (text_poke_copy(dst, src, len) == NULL)
		return ERR_PTR(-EINVAL);
	return dst;
}

 
bool bpf_jit_supports_subprog_tailcalls(void)
{
	return true;
}

void bpf_jit_free(struct bpf_prog *prog)
{
	if (prog->jited) {
		struct x64_jit_data *jit_data = prog->aux->jit_data;
		struct bpf_binary_header *hdr;

		 
		if (jit_data) {
			bpf_jit_binary_pack_finalize(prog, jit_data->header,
						     jit_data->rw_header);
			kvfree(jit_data->addrs);
			kfree(jit_data);
		}
		hdr = bpf_jit_binary_pack_hdr(prog);
		bpf_jit_binary_pack_free(hdr, NULL);
		WARN_ON_ONCE(!bpf_prog_kallsyms_verify_off(prog));
	}

	bpf_prog_unlock_free(prog);
}

void bpf_arch_poke_desc_update(struct bpf_jit_poke_descriptor *poke,
			       struct bpf_prog *new, struct bpf_prog *old)
{
	u8 *old_addr, *new_addr, *old_bypass_addr;
	int ret;

	old_bypass_addr = old ? NULL : poke->bypass_addr;
	old_addr = old ? (u8 *)old->bpf_func + poke->adj_off : NULL;
	new_addr = new ? (u8 *)new->bpf_func + poke->adj_off : NULL;

	 
	if (new) {
		ret = __bpf_arch_text_poke(poke->tailcall_target,
					   BPF_MOD_JUMP,
					   old_addr, new_addr);
		BUG_ON(ret < 0);
		if (!old) {
			ret = __bpf_arch_text_poke(poke->tailcall_bypass,
						   BPF_MOD_JUMP,
						   poke->bypass_addr,
						   NULL);
			BUG_ON(ret < 0);
		}
	} else {
		ret = __bpf_arch_text_poke(poke->tailcall_bypass,
					   BPF_MOD_JUMP,
					   old_bypass_addr,
					   poke->bypass_addr);
		BUG_ON(ret < 0);
		 
		if (!ret)
			synchronize_rcu();
		ret = __bpf_arch_text_poke(poke->tailcall_target,
					   BPF_MOD_JUMP,
					   old_addr, NULL);
		BUG_ON(ret < 0);
	}
}
