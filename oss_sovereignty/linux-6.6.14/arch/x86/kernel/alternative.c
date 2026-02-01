
#define pr_fmt(fmt) "SMP alternatives: " fmt

#include <linux/module.h>
#include <linux/sched.h>
#include <linux/perf_event.h>
#include <linux/mutex.h>
#include <linux/list.h>
#include <linux/stringify.h>
#include <linux/highmem.h>
#include <linux/mm.h>
#include <linux/vmalloc.h>
#include <linux/memory.h>
#include <linux/stop_machine.h>
#include <linux/slab.h>
#include <linux/kdebug.h>
#include <linux/kprobes.h>
#include <linux/mmu_context.h>
#include <linux/bsearch.h>
#include <linux/sync_core.h>
#include <asm/text-patching.h>
#include <asm/alternative.h>
#include <asm/sections.h>
#include <asm/mce.h>
#include <asm/nmi.h>
#include <asm/cacheflush.h>
#include <asm/tlbflush.h>
#include <asm/insn.h>
#include <asm/io.h>
#include <asm/fixmap.h>
#include <asm/paravirt.h>
#include <asm/asm-prototypes.h>

int __read_mostly alternatives_patched;

EXPORT_SYMBOL_GPL(alternatives_patched);

#define MAX_PATCH_LEN (255-1)

#define DA_ALL		(~0)
#define DA_ALT		0x01
#define DA_RET		0x02
#define DA_RETPOLINE	0x04
#define DA_ENDBR	0x08
#define DA_SMP		0x10

static unsigned int __initdata_or_module debug_alternative;

static int __init debug_alt(char *str)
{
	if (str && *str == '=')
		str++;

	if (!str || kstrtouint(str, 0, &debug_alternative))
		debug_alternative = DA_ALL;

	return 1;
}
__setup("debug-alternative", debug_alt);

static int noreplace_smp;

static int __init setup_noreplace_smp(char *str)
{
	noreplace_smp = 1;
	return 1;
}
__setup("noreplace-smp", setup_noreplace_smp);

#define DPRINTK(type, fmt, args...)					\
do {									\
	if (debug_alternative & DA_##type)				\
		printk(KERN_DEBUG pr_fmt(fmt) "\n", ##args);		\
} while (0)

#define DUMP_BYTES(type, buf, len, fmt, args...)			\
do {									\
	if (unlikely(debug_alternative & DA_##type)) {			\
		int j;							\
									\
		if (!(len))						\
			break;						\
									\
		printk(KERN_DEBUG pr_fmt(fmt), ##args);			\
		for (j = 0; j < (len) - 1; j++)				\
			printk(KERN_CONT "%02hhx ", buf[j]);		\
		printk(KERN_CONT "%02hhx\n", buf[j]);			\
	}								\
} while (0)

static const unsigned char x86nops[] =
{
	BYTES_NOP1,
	BYTES_NOP2,
	BYTES_NOP3,
	BYTES_NOP4,
	BYTES_NOP5,
	BYTES_NOP6,
	BYTES_NOP7,
	BYTES_NOP8,
#ifdef CONFIG_64BIT
	BYTES_NOP9,
	BYTES_NOP10,
	BYTES_NOP11,
#endif
};

const unsigned char * const x86_nops[ASM_NOP_MAX+1] =
{
	NULL,
	x86nops,
	x86nops + 1,
	x86nops + 1 + 2,
	x86nops + 1 + 2 + 3,
	x86nops + 1 + 2 + 3 + 4,
	x86nops + 1 + 2 + 3 + 4 + 5,
	x86nops + 1 + 2 + 3 + 4 + 5 + 6,
	x86nops + 1 + 2 + 3 + 4 + 5 + 6 + 7,
#ifdef CONFIG_64BIT
	x86nops + 1 + 2 + 3 + 4 + 5 + 6 + 7 + 8,
	x86nops + 1 + 2 + 3 + 4 + 5 + 6 + 7 + 8 + 9,
	x86nops + 1 + 2 + 3 + 4 + 5 + 6 + 7 + 8 + 9 + 10,
#endif
};

 
static void __init_or_module add_nop(u8 *instr, unsigned int len)
{
	u8 *target = instr + len;

	if (!len)
		return;

	if (len <= ASM_NOP_MAX) {
		memcpy(instr, x86_nops[len], len);
		return;
	}

	if (len < 128) {
		__text_gen_insn(instr, JMP8_INSN_OPCODE, instr, target, JMP8_INSN_SIZE);
		instr += JMP8_INSN_SIZE;
	} else {
		__text_gen_insn(instr, JMP32_INSN_OPCODE, instr, target, JMP32_INSN_SIZE);
		instr += JMP32_INSN_SIZE;
	}

	for (;instr < target; instr++)
		*instr = INT3_INSN_OPCODE;
}

extern s32 __retpoline_sites[], __retpoline_sites_end[];
extern s32 __return_sites[], __return_sites_end[];
extern s32 __cfi_sites[], __cfi_sites_end[];
extern s32 __ibt_endbr_seal[], __ibt_endbr_seal_end[];
extern struct alt_instr __alt_instructions[], __alt_instructions_end[];
extern s32 __smp_locks[], __smp_locks_end[];
void text_poke_early(void *addr, const void *opcode, size_t len);

 
static bool insn_is_nop(struct insn *insn)
{
	 
	if (insn->opcode.bytes[0] == 0x90 &&
	    (!insn->prefixes.nbytes || insn->prefixes.bytes[0] != 0xF3))
		return true;

	 
	if (insn->opcode.bytes[0] == 0x0F && insn->opcode.bytes[1] == 0x1F)
		return true;

	 

	return false;
}

 
static int skip_nops(u8 *instr, int offset, int len)
{
	struct insn insn;

	for (; offset < len; offset += insn.length) {
		if (insn_decode_kernel(&insn, &instr[offset]))
			break;

		if (!insn_is_nop(&insn))
			break;
	}

	return offset;
}

 
static bool __init_or_module
__optimize_nops(u8 *instr, size_t len, struct insn *insn, int *next, int *prev, int *target)
{
	int i = *next - insn->length;

	switch (insn->opcode.bytes[0]) {
	case JMP8_INSN_OPCODE:
	case JMP32_INSN_OPCODE:
		*prev = i;
		*target = *next + insn->immediate.value;
		return false;
	}

	if (insn_is_nop(insn)) {
		int nop = i;

		*next = skip_nops(instr, *next, len);
		if (*target && *next == *target)
			nop = *prev;

		add_nop(instr + nop, *next - nop);
		DUMP_BYTES(ALT, instr, len, "%px: [%d:%d) optimized NOPs: ", instr, nop, *next);
		return true;
	}

	*target = 0;
	return false;
}

 
static void __init_or_module noinline optimize_nops(u8 *instr, size_t len)
{
	int prev, target = 0;

	for (int next, i = 0; i < len; i = next) {
		struct insn insn;

		if (insn_decode_kernel(&insn, &instr[i]))
			return;

		next = i + insn.length;

		__optimize_nops(instr, len, &insn, &next, &prev, &target);
	}
}

static void __init_or_module noinline optimize_nops_inplace(u8 *instr, size_t len)
{
	unsigned long flags;

	local_irq_save(flags);
	optimize_nops(instr, len);
	sync_core();
	local_irq_restore(flags);
}

 

#define apply_reloc_n(n_, p_, d_)				\
	do {							\
		s32 v = *(s##n_ *)(p_);				\
		v += (d_);					\
		BUG_ON((v >> 31) != (v >> (n_-1)));		\
		*(s##n_ *)(p_) = (s##n_)v;			\
	} while (0)


static __always_inline
void apply_reloc(int n, void *ptr, uintptr_t diff)
{
	switch (n) {
	case 1: apply_reloc_n(8, ptr, diff); break;
	case 2: apply_reloc_n(16, ptr, diff); break;
	case 4: apply_reloc_n(32, ptr, diff); break;
	default: BUG();
	}
}

static __always_inline
bool need_reloc(unsigned long offset, u8 *src, size_t src_len)
{
	u8 *target = src + offset;
	 
	return (target < src || target > src + src_len);
}

static void __init_or_module noinline
apply_relocation(u8 *buf, size_t len, u8 *dest, u8 *src, size_t src_len)
{
	int prev, target = 0;

	for (int next, i = 0; i < len; i = next) {
		struct insn insn;

		if (WARN_ON_ONCE(insn_decode_kernel(&insn, &buf[i])))
			return;

		next = i + insn.length;

		if (__optimize_nops(buf, len, &insn, &next, &prev, &target))
			continue;

		switch (insn.opcode.bytes[0]) {
		case 0x0f:
			if (insn.opcode.bytes[1] < 0x80 ||
			    insn.opcode.bytes[1] > 0x8f)
				break;

			fallthrough;	 
		case 0x70 ... 0x7f:	 
		case JMP8_INSN_OPCODE:
		case JMP32_INSN_OPCODE:
		case CALL_INSN_OPCODE:
			if (need_reloc(next + insn.immediate.value, src, src_len)) {
				apply_reloc(insn.immediate.nbytes,
					    buf + i + insn_offset_immediate(&insn),
					    src - dest);
			}

			 
			if (insn.opcode.bytes[0] == JMP32_INSN_OPCODE) {
				s32 imm = insn.immediate.value;
				imm += src - dest;
				imm += JMP32_INSN_SIZE - JMP8_INSN_SIZE;
				if ((imm >> 31) == (imm >> 7)) {
					buf[i+0] = JMP8_INSN_OPCODE;
					buf[i+1] = (s8)imm;

					memset(&buf[i+2], INT3_INSN_OPCODE, insn.length - 2);
				}
			}
			break;
		}

		if (insn_rip_relative(&insn)) {
			if (need_reloc(next + insn.displacement.value, src, src_len)) {
				apply_reloc(insn.displacement.nbytes,
					    buf + i + insn_offset_displacement(&insn),
					    src - dest);
			}
		}
	}
}

 
void __init_or_module noinline apply_alternatives(struct alt_instr *start,
						  struct alt_instr *end)
{
	struct alt_instr *a;
	u8 *instr, *replacement;
	u8 insn_buff[MAX_PATCH_LEN];

	DPRINTK(ALT, "alt table %px, -> %px", start, end);

	 
	kasan_disable_current();

	 
	for (a = start; a < end; a++) {
		int insn_buff_sz = 0;

		instr = (u8 *)&a->instr_offset + a->instr_offset;
		replacement = (u8 *)&a->repl_offset + a->repl_offset;
		BUG_ON(a->instrlen > sizeof(insn_buff));
		BUG_ON(a->cpuid >= (NCAPINTS + NBUGINTS) * 32);

		 
		if (!boot_cpu_has(a->cpuid) == !(a->flags & ALT_FLAG_NOT)) {
			optimize_nops_inplace(instr, a->instrlen);
			continue;
		}

		DPRINTK(ALT, "feat: %s%d*32+%d, old: (%pS (%px) len: %d), repl: (%px, len: %d)",
			(a->flags & ALT_FLAG_NOT) ? "!" : "",
			a->cpuid >> 5,
			a->cpuid & 0x1f,
			instr, instr, a->instrlen,
			replacement, a->replacementlen);

		memcpy(insn_buff, replacement, a->replacementlen);
		insn_buff_sz = a->replacementlen;

		for (; insn_buff_sz < a->instrlen; insn_buff_sz++)
			insn_buff[insn_buff_sz] = 0x90;

		apply_relocation(insn_buff, a->instrlen, instr, replacement, a->replacementlen);

		DUMP_BYTES(ALT, instr, a->instrlen, "%px:   old_insn: ", instr);
		DUMP_BYTES(ALT, replacement, a->replacementlen, "%px:   rpl_insn: ", replacement);
		DUMP_BYTES(ALT, insn_buff, insn_buff_sz, "%px: final_insn: ", instr);

		text_poke_early(instr, insn_buff, insn_buff_sz);
	}

	kasan_enable_current();
}

static inline bool is_jcc32(struct insn *insn)
{
	 
	return insn->opcode.bytes[0] == 0x0f && (insn->opcode.bytes[1] & 0xf0) == 0x80;
}

#if defined(CONFIG_RETPOLINE) && defined(CONFIG_OBJTOOL)

 
static int emit_indirect(int op, int reg, u8 *bytes)
{
	int i = 0;
	u8 modrm;

	switch (op) {
	case CALL_INSN_OPCODE:
		modrm = 0x10;  
		break;

	case JMP32_INSN_OPCODE:
		modrm = 0x20;  
		break;

	default:
		WARN_ON_ONCE(1);
		return -1;
	}

	if (reg >= 8) {
		bytes[i++] = 0x41;  
		reg -= 8;
	}

	modrm |= 0xc0;  
	modrm += reg;

	bytes[i++] = 0xff;  
	bytes[i++] = modrm;

	return i;
}

static int emit_call_track_retpoline(void *addr, struct insn *insn, int reg, u8 *bytes)
{
	u8 op = insn->opcode.bytes[0];
	int i = 0;

	 
	if (is_jcc32(insn)) {
		bytes[i++] = op;
		op = insn->opcode.bytes[1];
		goto clang_jcc;
	}

	if (insn->length == 6)
		bytes[i++] = 0x2e;  

	switch (op) {
	case CALL_INSN_OPCODE:
		__text_gen_insn(bytes+i, op, addr+i,
				__x86_indirect_call_thunk_array[reg],
				CALL_INSN_SIZE);
		i += CALL_INSN_SIZE;
		break;

	case JMP32_INSN_OPCODE:
clang_jcc:
		__text_gen_insn(bytes+i, op, addr+i,
				__x86_indirect_jump_thunk_array[reg],
				JMP32_INSN_SIZE);
		i += JMP32_INSN_SIZE;
		break;

	default:
		WARN(1, "%pS %px %*ph\n", addr, addr, 6, addr);
		return -1;
	}

	WARN_ON_ONCE(i != insn->length);

	return i;
}

 
static int patch_retpoline(void *addr, struct insn *insn, u8 *bytes)
{
	retpoline_thunk_t *target;
	int reg, ret, i = 0;
	u8 op, cc;

	target = addr + insn->length + insn->immediate.value;
	reg = target - __x86_indirect_thunk_array;

	if (WARN_ON_ONCE(reg & ~0xf))
		return -1;

	 
	BUG_ON(reg == 4);

	if (cpu_feature_enabled(X86_FEATURE_RETPOLINE) &&
	    !cpu_feature_enabled(X86_FEATURE_RETPOLINE_LFENCE)) {
		if (cpu_feature_enabled(X86_FEATURE_CALL_DEPTH))
			return emit_call_track_retpoline(addr, insn, reg, bytes);

		return -1;
	}

	op = insn->opcode.bytes[0];

	 
	if (is_jcc32(insn)) {
		cc = insn->opcode.bytes[1] & 0xf;
		cc ^= 1;  

		bytes[i++] = 0x70 + cc;         
		bytes[i++] = insn->length - 2;  

		 
		op = JMP32_INSN_OPCODE;
	}

	 
	if (cpu_feature_enabled(X86_FEATURE_RETPOLINE_LFENCE)) {
		bytes[i++] = 0x0f;
		bytes[i++] = 0xae;
		bytes[i++] = 0xe8;  
	}

	ret = emit_indirect(op, reg, bytes + i);
	if (ret < 0)
		return ret;
	i += ret;

	 
	if (op == JMP32_INSN_OPCODE && i < insn->length)
		bytes[i++] = INT3_INSN_OPCODE;

	for (; i < insn->length;)
		bytes[i++] = BYTES_NOP1;

	return i;
}

 
void __init_or_module noinline apply_retpolines(s32 *start, s32 *end)
{
	s32 *s;

	for (s = start; s < end; s++) {
		void *addr = (void *)s + *s;
		struct insn insn;
		int len, ret;
		u8 bytes[16];
		u8 op1, op2;

		ret = insn_decode_kernel(&insn, addr);
		if (WARN_ON_ONCE(ret < 0))
			continue;

		op1 = insn.opcode.bytes[0];
		op2 = insn.opcode.bytes[1];

		switch (op1) {
		case CALL_INSN_OPCODE:
		case JMP32_INSN_OPCODE:
			break;

		case 0x0f:  
			if (op2 >= 0x80 && op2 <= 0x8f)
				break;
			fallthrough;
		default:
			WARN_ON_ONCE(1);
			continue;
		}

		DPRINTK(RETPOLINE, "retpoline at: %pS (%px) len: %d to: %pS",
			addr, addr, insn.length,
			addr + insn.length + insn.immediate.value);

		len = patch_retpoline(addr, &insn, bytes);
		if (len == insn.length) {
			optimize_nops(bytes, len);
			DUMP_BYTES(RETPOLINE, ((u8*)addr),  len, "%px: orig: ", addr);
			DUMP_BYTES(RETPOLINE, ((u8*)bytes), len, "%px: repl: ", addr);
			text_poke_early(addr, bytes, len);
		}
	}
}

#ifdef CONFIG_RETHUNK

 
static int patch_return(void *addr, struct insn *insn, u8 *bytes)
{
	int i = 0;

	 
	if (cpu_feature_enabled(X86_FEATURE_RETHUNK)) {
		i = JMP32_INSN_SIZE;
		__text_gen_insn(bytes, JMP32_INSN_OPCODE, addr, x86_return_thunk, i);
	} else {
		 
		bytes[i++] = RET_INSN_OPCODE;
	}

	for (; i < insn->length;)
		bytes[i++] = INT3_INSN_OPCODE;
	return i;
}

void __init_or_module noinline apply_returns(s32 *start, s32 *end)
{
	s32 *s;

	if (cpu_feature_enabled(X86_FEATURE_RETHUNK))
		static_call_force_reinit();

	for (s = start; s < end; s++) {
		void *dest = NULL, *addr = (void *)s + *s;
		struct insn insn;
		int len, ret;
		u8 bytes[16];
		u8 op;

		ret = insn_decode_kernel(&insn, addr);
		if (WARN_ON_ONCE(ret < 0))
			continue;

		op = insn.opcode.bytes[0];
		if (op == JMP32_INSN_OPCODE)
			dest = addr + insn.length + insn.immediate.value;

		if (__static_call_fixup(addr, op, dest) ||
		    WARN_ONCE(dest != &__x86_return_thunk,
			      "missing return thunk: %pS-%pS: %*ph",
			      addr, dest, 5, addr))
			continue;

		DPRINTK(RET, "return thunk at: %pS (%px) len: %d to: %pS",
			addr, addr, insn.length,
			addr + insn.length + insn.immediate.value);

		len = patch_return(addr, &insn, bytes);
		if (len == insn.length) {
			DUMP_BYTES(RET, ((u8*)addr),  len, "%px: orig: ", addr);
			DUMP_BYTES(RET, ((u8*)bytes), len, "%px: repl: ", addr);
			text_poke_early(addr, bytes, len);
		}
	}
}
#else
void __init_or_module noinline apply_returns(s32 *start, s32 *end) { }
#endif  

#else  

void __init_or_module noinline apply_retpolines(s32 *start, s32 *end) { }
void __init_or_module noinline apply_returns(s32 *start, s32 *end) { }

#endif  

#ifdef CONFIG_X86_KERNEL_IBT

static void poison_cfi(void *addr);

static void __init_or_module poison_endbr(void *addr, bool warn)
{
	u32 endbr, poison = gen_endbr_poison();

	if (WARN_ON_ONCE(get_kernel_nofault(endbr, addr)))
		return;

	if (!is_endbr(endbr)) {
		WARN_ON_ONCE(warn);
		return;
	}

	DPRINTK(ENDBR, "ENDBR at: %pS (%px)", addr, addr);

	 
	DUMP_BYTES(ENDBR, ((u8*)addr), 4, "%px: orig: ", addr);
	DUMP_BYTES(ENDBR, ((u8*)&poison), 4, "%px: repl: ", addr);
	text_poke_early(addr, &poison, 4);
}

 
void __init_or_module noinline apply_seal_endbr(s32 *start, s32 *end)
{
	s32 *s;

	for (s = start; s < end; s++) {
		void *addr = (void *)s + *s;

		poison_endbr(addr, true);
		if (IS_ENABLED(CONFIG_FINEIBT))
			poison_cfi(addr - 16);
	}
}

#else

void __init_or_module apply_seal_endbr(s32 *start, s32 *end) { }

#endif  

#ifdef CONFIG_FINEIBT

enum cfi_mode {
	CFI_DEFAULT,
	CFI_OFF,
	CFI_KCFI,
	CFI_FINEIBT,
};

static enum cfi_mode cfi_mode __ro_after_init = CFI_DEFAULT;
static bool cfi_rand __ro_after_init = true;
static u32  cfi_seed __ro_after_init;

 
static u32 cfi_rehash(u32 hash)
{
	hash ^= cfi_seed;
	while (unlikely(is_endbr(hash) || is_endbr(-hash))) {
		bool lsb = hash & 1;
		hash >>= 1;
		if (lsb)
			hash ^= 0x80200003;
	}
	return hash;
}

static __init int cfi_parse_cmdline(char *str)
{
	if (!str)
		return -EINVAL;

	while (str) {
		char *next = strchr(str, ',');
		if (next) {
			*next = 0;
			next++;
		}

		if (!strcmp(str, "auto")) {
			cfi_mode = CFI_DEFAULT;
		} else if (!strcmp(str, "off")) {
			cfi_mode = CFI_OFF;
			cfi_rand = false;
		} else if (!strcmp(str, "kcfi")) {
			cfi_mode = CFI_KCFI;
		} else if (!strcmp(str, "fineibt")) {
			cfi_mode = CFI_FINEIBT;
		} else if (!strcmp(str, "norand")) {
			cfi_rand = false;
		} else {
			pr_err("Ignoring unknown cfi option (%s).", str);
		}

		str = next;
	}

	return 0;
}
early_param("cfi", cfi_parse_cmdline);

 

asm(	".pushsection .rodata			\n"
	"fineibt_preamble_start:		\n"
	"	endbr64				\n"
	"	subl	$0x12345678, %r10d	\n"
	"	je	fineibt_preamble_end	\n"
	"	ud2				\n"
	"	nop				\n"
	"fineibt_preamble_end:			\n"
	".popsection\n"
);

extern u8 fineibt_preamble_start[];
extern u8 fineibt_preamble_end[];

#define fineibt_preamble_size (fineibt_preamble_end - fineibt_preamble_start)
#define fineibt_preamble_hash 7

asm(	".pushsection .rodata			\n"
	"fineibt_caller_start:			\n"
	"	movl	$0x12345678, %r10d	\n"
	"	sub	$16, %r11		\n"
	ASM_NOP4
	"fineibt_caller_end:			\n"
	".popsection				\n"
);

extern u8 fineibt_caller_start[];
extern u8 fineibt_caller_end[];

#define fineibt_caller_size (fineibt_caller_end - fineibt_caller_start)
#define fineibt_caller_hash 2

#define fineibt_caller_jmp (fineibt_caller_size - 2)

static u32 decode_preamble_hash(void *addr)
{
	u8 *p = addr;

	 
	if (p[0] == 0xb8)
		return *(u32 *)(addr + 1);

	return 0;  
}

static u32 decode_caller_hash(void *addr)
{
	u8 *p = addr;

	 
	if (p[0] == 0x41 && p[1] == 0xba)
		return -*(u32 *)(addr + 2);

	 
	if (p[0] == JMP8_INSN_OPCODE && p[1] == fineibt_caller_jmp)
		return -*(u32 *)(addr + 2);

	return 0;  
}

 
static int cfi_disable_callers(s32 *start, s32 *end)
{
	 
	const u8 jmp[] = { JMP8_INSN_OPCODE, fineibt_caller_jmp };
	s32 *s;

	for (s = start; s < end; s++) {
		void *addr = (void *)s + *s;
		u32 hash;

		addr -= fineibt_caller_size;
		hash = decode_caller_hash(addr);
		if (!hash)  
			continue;

		text_poke_early(addr, jmp, 2);
	}

	return 0;
}

static int cfi_enable_callers(s32 *start, s32 *end)
{
	 
	const u8 mov[] = { 0x41, 0xba };
	s32 *s;

	for (s = start; s < end; s++) {
		void *addr = (void *)s + *s;
		u32 hash;

		addr -= fineibt_caller_size;
		hash = decode_caller_hash(addr);
		if (!hash)  
			continue;

		text_poke_early(addr, mov, 2);
	}

	return 0;
}

 
static int cfi_rand_preamble(s32 *start, s32 *end)
{
	s32 *s;

	for (s = start; s < end; s++) {
		void *addr = (void *)s + *s;
		u32 hash;

		hash = decode_preamble_hash(addr);
		if (WARN(!hash, "no CFI hash found at: %pS %px %*ph\n",
			 addr, addr, 5, addr))
			return -EINVAL;

		hash = cfi_rehash(hash);
		text_poke_early(addr + 1, &hash, 4);
	}

	return 0;
}

static int cfi_rewrite_preamble(s32 *start, s32 *end)
{
	s32 *s;

	for (s = start; s < end; s++) {
		void *addr = (void *)s + *s;
		u32 hash;

		hash = decode_preamble_hash(addr);
		if (WARN(!hash, "no CFI hash found at: %pS %px %*ph\n",
			 addr, addr, 5, addr))
			return -EINVAL;

		text_poke_early(addr, fineibt_preamble_start, fineibt_preamble_size);
		WARN_ON(*(u32 *)(addr + fineibt_preamble_hash) != 0x12345678);
		text_poke_early(addr + fineibt_preamble_hash, &hash, 4);
	}

	return 0;
}

static void cfi_rewrite_endbr(s32 *start, s32 *end)
{
	s32 *s;

	for (s = start; s < end; s++) {
		void *addr = (void *)s + *s;

		poison_endbr(addr+16, false);
	}
}

 
static int cfi_rand_callers(s32 *start, s32 *end)
{
	s32 *s;

	for (s = start; s < end; s++) {
		void *addr = (void *)s + *s;
		u32 hash;

		addr -= fineibt_caller_size;
		hash = decode_caller_hash(addr);
		if (hash) {
			hash = -cfi_rehash(hash);
			text_poke_early(addr + 2, &hash, 4);
		}
	}

	return 0;
}

static int cfi_rewrite_callers(s32 *start, s32 *end)
{
	s32 *s;

	for (s = start; s < end; s++) {
		void *addr = (void *)s + *s;
		u32 hash;

		addr -= fineibt_caller_size;
		hash = decode_caller_hash(addr);
		if (hash) {
			text_poke_early(addr, fineibt_caller_start, fineibt_caller_size);
			WARN_ON(*(u32 *)(addr + fineibt_caller_hash) != 0x12345678);
			text_poke_early(addr + fineibt_caller_hash, &hash, 4);
		}
		 
	}

	return 0;
}

static void __apply_fineibt(s32 *start_retpoline, s32 *end_retpoline,
			    s32 *start_cfi, s32 *end_cfi, bool builtin)
{
	int ret;

	if (WARN_ONCE(fineibt_preamble_size != 16,
		      "FineIBT preamble wrong size: %ld", fineibt_preamble_size))
		return;

	if (cfi_mode == CFI_DEFAULT) {
		cfi_mode = CFI_KCFI;
		if (HAS_KERNEL_IBT && cpu_feature_enabled(X86_FEATURE_IBT))
			cfi_mode = CFI_FINEIBT;
	}

	 
	ret = cfi_disable_callers(start_retpoline, end_retpoline);
	if (ret)
		goto err;

	if (cfi_rand) {
		if (builtin)
			cfi_seed = get_random_u32();

		ret = cfi_rand_preamble(start_cfi, end_cfi);
		if (ret)
			goto err;

		ret = cfi_rand_callers(start_retpoline, end_retpoline);
		if (ret)
			goto err;
	}

	switch (cfi_mode) {
	case CFI_OFF:
		if (builtin)
			pr_info("Disabling CFI\n");
		return;

	case CFI_KCFI:
		ret = cfi_enable_callers(start_retpoline, end_retpoline);
		if (ret)
			goto err;

		if (builtin)
			pr_info("Using kCFI\n");
		return;

	case CFI_FINEIBT:
		 
		ret = cfi_rewrite_preamble(start_cfi, end_cfi);
		if (ret)
			goto err;

		 
		ret = cfi_rewrite_callers(start_retpoline, end_retpoline);
		if (ret)
			goto err;

		 
		cfi_rewrite_endbr(start_cfi, end_cfi);

		if (builtin)
			pr_info("Using FineIBT CFI\n");
		return;

	default:
		break;
	}

err:
	pr_err("Something went horribly wrong trying to rewrite the CFI implementation.\n");
}

static inline void poison_hash(void *addr)
{
	*(u32 *)addr = 0;
}

static void poison_cfi(void *addr)
{
	switch (cfi_mode) {
	case CFI_FINEIBT:
		 
		poison_endbr(addr, false);
		poison_hash(addr + fineibt_preamble_hash);
		break;

	case CFI_KCFI:
		 
		poison_hash(addr + 1);
		break;

	default:
		break;
	}
}

#else

static void __apply_fineibt(s32 *start_retpoline, s32 *end_retpoline,
			    s32 *start_cfi, s32 *end_cfi, bool builtin)
{
}

#ifdef CONFIG_X86_KERNEL_IBT
static void poison_cfi(void *addr) { }
#endif

#endif

void apply_fineibt(s32 *start_retpoline, s32 *end_retpoline,
		   s32 *start_cfi, s32 *end_cfi)
{
	return __apply_fineibt(start_retpoline, end_retpoline,
			       start_cfi, end_cfi,
			         false);
}

#ifdef CONFIG_SMP
static void alternatives_smp_lock(const s32 *start, const s32 *end,
				  u8 *text, u8 *text_end)
{
	const s32 *poff;

	for (poff = start; poff < end; poff++) {
		u8 *ptr = (u8 *)poff + *poff;

		if (!*poff || ptr < text || ptr >= text_end)
			continue;
		 
		if (*ptr == 0x3e)
			text_poke(ptr, ((unsigned char []){0xf0}), 1);
	}
}

static void alternatives_smp_unlock(const s32 *start, const s32 *end,
				    u8 *text, u8 *text_end)
{
	const s32 *poff;

	for (poff = start; poff < end; poff++) {
		u8 *ptr = (u8 *)poff + *poff;

		if (!*poff || ptr < text || ptr >= text_end)
			continue;
		 
		if (*ptr == 0xf0)
			text_poke(ptr, ((unsigned char []){0x3E}), 1);
	}
}

struct smp_alt_module {
	 
	struct module	*mod;
	char		*name;

	 
	const s32	*locks;
	const s32	*locks_end;

	 
	u8		*text;
	u8		*text_end;

	struct list_head next;
};
static LIST_HEAD(smp_alt_modules);
static bool uniproc_patched = false;	 

void __init_or_module alternatives_smp_module_add(struct module *mod,
						  char *name,
						  void *locks, void *locks_end,
						  void *text,  void *text_end)
{
	struct smp_alt_module *smp;

	mutex_lock(&text_mutex);
	if (!uniproc_patched)
		goto unlock;

	if (num_possible_cpus() == 1)
		 
		goto smp_unlock;

	smp = kzalloc(sizeof(*smp), GFP_KERNEL);
	if (NULL == smp)
		 
		goto unlock;

	smp->mod	= mod;
	smp->name	= name;
	smp->locks	= locks;
	smp->locks_end	= locks_end;
	smp->text	= text;
	smp->text_end	= text_end;
	DPRINTK(SMP, "locks %p -> %p, text %p -> %p, name %s\n",
		smp->locks, smp->locks_end,
		smp->text, smp->text_end, smp->name);

	list_add_tail(&smp->next, &smp_alt_modules);
smp_unlock:
	alternatives_smp_unlock(locks, locks_end, text, text_end);
unlock:
	mutex_unlock(&text_mutex);
}

void __init_or_module alternatives_smp_module_del(struct module *mod)
{
	struct smp_alt_module *item;

	mutex_lock(&text_mutex);
	list_for_each_entry(item, &smp_alt_modules, next) {
		if (mod != item->mod)
			continue;
		list_del(&item->next);
		kfree(item);
		break;
	}
	mutex_unlock(&text_mutex);
}

void alternatives_enable_smp(void)
{
	struct smp_alt_module *mod;

	 
	BUG_ON(num_possible_cpus() == 1);

	mutex_lock(&text_mutex);

	if (uniproc_patched) {
		pr_info("switching to SMP code\n");
		BUG_ON(num_online_cpus() != 1);
		clear_cpu_cap(&boot_cpu_data, X86_FEATURE_UP);
		clear_cpu_cap(&cpu_data(0), X86_FEATURE_UP);
		list_for_each_entry(mod, &smp_alt_modules, next)
			alternatives_smp_lock(mod->locks, mod->locks_end,
					      mod->text, mod->text_end);
		uniproc_patched = false;
	}
	mutex_unlock(&text_mutex);
}

 
int alternatives_text_reserved(void *start, void *end)
{
	struct smp_alt_module *mod;
	const s32 *poff;
	u8 *text_start = start;
	u8 *text_end = end;

	lockdep_assert_held(&text_mutex);

	list_for_each_entry(mod, &smp_alt_modules, next) {
		if (mod->text > text_end || mod->text_end < text_start)
			continue;
		for (poff = mod->locks; poff < mod->locks_end; poff++) {
			const u8 *ptr = (const u8 *)poff + *poff;

			if (text_start <= ptr && text_end > ptr)
				return 1;
		}
	}

	return 0;
}
#endif  

#ifdef CONFIG_PARAVIRT

 
static void __init_or_module add_nops(void *insns, unsigned int len)
{
	while (len > 0) {
		unsigned int noplen = len;
		if (noplen > ASM_NOP_MAX)
			noplen = ASM_NOP_MAX;
		memcpy(insns, x86_nops[noplen], noplen);
		insns += noplen;
		len -= noplen;
	}
}

void __init_or_module apply_paravirt(struct paravirt_patch_site *start,
				     struct paravirt_patch_site *end)
{
	struct paravirt_patch_site *p;
	char insn_buff[MAX_PATCH_LEN];

	for (p = start; p < end; p++) {
		unsigned int used;

		BUG_ON(p->len > MAX_PATCH_LEN);
		 
		memcpy(insn_buff, p->instr, p->len);
		used = paravirt_patch(p->type, insn_buff, (unsigned long)p->instr, p->len);

		BUG_ON(used > p->len);

		 
		add_nops(insn_buff + used, p->len - used);
		text_poke_early(p->instr, insn_buff, p->len);
	}
}
extern struct paravirt_patch_site __start_parainstructions[],
	__stop_parainstructions[];
#endif	 

 

 

extern void int3_magic(unsigned int *ptr);  

asm (
"	.pushsection	.init.text, \"ax\", @progbits\n"
"	.type		int3_magic, @function\n"
"int3_magic:\n"
	ANNOTATE_NOENDBR
"	movl	$1, (%" _ASM_ARG1 ")\n"
	ASM_RET
"	.size		int3_magic, .-int3_magic\n"
"	.popsection\n"
);

extern void int3_selftest_ip(void);  

static int __init
int3_exception_notify(struct notifier_block *self, unsigned long val, void *data)
{
	unsigned long selftest = (unsigned long)&int3_selftest_ip;
	struct die_args *args = data;
	struct pt_regs *regs = args->regs;

	OPTIMIZER_HIDE_VAR(selftest);

	if (!regs || user_mode(regs))
		return NOTIFY_DONE;

	if (val != DIE_INT3)
		return NOTIFY_DONE;

	if (regs->ip - INT3_INSN_SIZE != selftest)
		return NOTIFY_DONE;

	int3_emulate_call(regs, (unsigned long)&int3_magic);
	return NOTIFY_STOP;
}

 
static noinline void __init int3_selftest(void)
{
	static __initdata struct notifier_block int3_exception_nb = {
		.notifier_call	= int3_exception_notify,
		.priority	= INT_MAX-1,  
	};
	unsigned int val = 0;

	BUG_ON(register_die_notifier(&int3_exception_nb));

	 
	asm volatile ("int3_selftest_ip:\n\t"
		      ANNOTATE_NOENDBR
		      "    int3; nop; nop; nop; nop\n\t"
		      : ASM_CALL_CONSTRAINT
		      : __ASM_SEL_RAW(a, D) (&val)
		      : "memory");

	BUG_ON(val != 1);

	unregister_die_notifier(&int3_exception_nb);
}

static __initdata int __alt_reloc_selftest_addr;

extern void __init __alt_reloc_selftest(void *arg);
__visible noinline void __init __alt_reloc_selftest(void *arg)
{
	WARN_ON(arg != &__alt_reloc_selftest_addr);
}

static noinline void __init alt_reloc_selftest(void)
{
	 
	asm_inline volatile (
		ALTERNATIVE("", "lea %[mem], %%" _ASM_ARG1 "; call __alt_reloc_selftest;", X86_FEATURE_ALWAYS)
		:  
		: [mem] "m" (__alt_reloc_selftest_addr)
		: _ASM_ARG1
	);
}

void __init alternative_instructions(void)
{
	int3_selftest();

	 
	stop_nmi();

	 

	 
	paravirt_set_cap();

	 
	apply_paravirt(__parainstructions, __parainstructions_end);

	__apply_fineibt(__retpoline_sites, __retpoline_sites_end,
			__cfi_sites, __cfi_sites_end, true);

	 
	apply_retpolines(__retpoline_sites, __retpoline_sites_end);
	apply_returns(__return_sites, __return_sites_end);

	 
	apply_alternatives(__alt_instructions, __alt_instructions_end);

	 
	callthunks_patch_builtin_calls();

	 
	apply_seal_endbr(__ibt_endbr_seal, __ibt_endbr_seal_end);

#ifdef CONFIG_SMP
	 
	if (!noreplace_smp && (num_present_cpus() == 1 || setup_max_cpus <= 1)) {
		uniproc_patched = true;
		alternatives_smp_module_add(NULL, "core kernel",
					    __smp_locks, __smp_locks_end,
					    _text, _etext);
	}

	if (!uniproc_patched || num_possible_cpus() == 1) {
		free_init_pages("SMP alternatives",
				(unsigned long)__smp_locks,
				(unsigned long)__smp_locks_end);
	}
#endif

	restart_nmi();
	alternatives_patched = 1;

	alt_reloc_selftest();
}

 
void __init_or_module text_poke_early(void *addr, const void *opcode,
				      size_t len)
{
	unsigned long flags;

	if (boot_cpu_has(X86_FEATURE_NX) &&
	    is_module_text_address((unsigned long)addr)) {
		 
		memcpy(addr, opcode, len);
	} else {
		local_irq_save(flags);
		memcpy(addr, opcode, len);
		sync_core();
		local_irq_restore(flags);

		 
	}
}

typedef struct {
	struct mm_struct *mm;
} temp_mm_state_t;

 
static inline temp_mm_state_t use_temporary_mm(struct mm_struct *mm)
{
	temp_mm_state_t temp_state;

	lockdep_assert_irqs_disabled();

	 
	if (this_cpu_read(cpu_tlbstate_shared.is_lazy))
		leave_mm(smp_processor_id());

	temp_state.mm = this_cpu_read(cpu_tlbstate.loaded_mm);
	switch_mm_irqs_off(NULL, mm, current);

	 
	if (hw_breakpoint_active())
		hw_breakpoint_disable();

	return temp_state;
}

static inline void unuse_temporary_mm(temp_mm_state_t prev_state)
{
	lockdep_assert_irqs_disabled();
	switch_mm_irqs_off(NULL, prev_state.mm, current);

	 
	if (hw_breakpoint_active())
		hw_breakpoint_restore();
}

__ro_after_init struct mm_struct *poking_mm;
__ro_after_init unsigned long poking_addr;

static void text_poke_memcpy(void *dst, const void *src, size_t len)
{
	memcpy(dst, src, len);
}

static void text_poke_memset(void *dst, const void *src, size_t len)
{
	int c = *(const int *)src;

	memset(dst, c, len);
}

typedef void text_poke_f(void *dst, const void *src, size_t len);

static void *__text_poke(text_poke_f func, void *addr, const void *src, size_t len)
{
	bool cross_page_boundary = offset_in_page(addr) + len > PAGE_SIZE;
	struct page *pages[2] = {NULL};
	temp_mm_state_t prev;
	unsigned long flags;
	pte_t pte, *ptep;
	spinlock_t *ptl;
	pgprot_t pgprot;

	 
	BUG_ON(!after_bootmem);

	if (!core_kernel_text((unsigned long)addr)) {
		pages[0] = vmalloc_to_page(addr);
		if (cross_page_boundary)
			pages[1] = vmalloc_to_page(addr + PAGE_SIZE);
	} else {
		pages[0] = virt_to_page(addr);
		WARN_ON(!PageReserved(pages[0]));
		if (cross_page_boundary)
			pages[1] = virt_to_page(addr + PAGE_SIZE);
	}
	 
	BUG_ON(!pages[0] || (cross_page_boundary && !pages[1]));

	 
	pgprot = __pgprot(pgprot_val(PAGE_KERNEL) & ~_PAGE_GLOBAL);

	 
	ptep = get_locked_pte(poking_mm, poking_addr, &ptl);

	 
	VM_BUG_ON(!ptep);

	local_irq_save(flags);

	pte = mk_pte(pages[0], pgprot);
	set_pte_at(poking_mm, poking_addr, ptep, pte);

	if (cross_page_boundary) {
		pte = mk_pte(pages[1], pgprot);
		set_pte_at(poking_mm, poking_addr + PAGE_SIZE, ptep + 1, pte);
	}

	 
	prev = use_temporary_mm(poking_mm);

	kasan_disable_current();
	func((u8 *)poking_addr + offset_in_page(addr), src, len);
	kasan_enable_current();

	 
	barrier();

	pte_clear(poking_mm, poking_addr, ptep);
	if (cross_page_boundary)
		pte_clear(poking_mm, poking_addr + PAGE_SIZE, ptep + 1);

	 
	unuse_temporary_mm(prev);

	 
	flush_tlb_mm_range(poking_mm, poking_addr, poking_addr +
			   (cross_page_boundary ? 2 : 1) * PAGE_SIZE,
			   PAGE_SHIFT, false);

	if (func == text_poke_memcpy) {
		 
		BUG_ON(memcmp(addr, src, len));
	}

	local_irq_restore(flags);
	pte_unmap_unlock(ptep, ptl);
	return addr;
}

 
void *text_poke(void *addr, const void *opcode, size_t len)
{
	lockdep_assert_held(&text_mutex);

	return __text_poke(text_poke_memcpy, addr, opcode, len);
}

 
void *text_poke_kgdb(void *addr, const void *opcode, size_t len)
{
	return __text_poke(text_poke_memcpy, addr, opcode, len);
}

void *text_poke_copy_locked(void *addr, const void *opcode, size_t len,
			    bool core_ok)
{
	unsigned long start = (unsigned long)addr;
	size_t patched = 0;

	if (WARN_ON_ONCE(!core_ok && core_kernel_text(start)))
		return NULL;

	while (patched < len) {
		unsigned long ptr = start + patched;
		size_t s;

		s = min_t(size_t, PAGE_SIZE * 2 - offset_in_page(ptr), len - patched);

		__text_poke(text_poke_memcpy, (void *)ptr, opcode + patched, s);
		patched += s;
	}
	return addr;
}

 
void *text_poke_copy(void *addr, const void *opcode, size_t len)
{
	mutex_lock(&text_mutex);
	addr = text_poke_copy_locked(addr, opcode, len, false);
	mutex_unlock(&text_mutex);
	return addr;
}

 
void *text_poke_set(void *addr, int c, size_t len)
{
	unsigned long start = (unsigned long)addr;
	size_t patched = 0;

	if (WARN_ON_ONCE(core_kernel_text(start)))
		return NULL;

	mutex_lock(&text_mutex);
	while (patched < len) {
		unsigned long ptr = start + patched;
		size_t s;

		s = min_t(size_t, PAGE_SIZE * 2 - offset_in_page(ptr), len - patched);

		__text_poke(text_poke_memset, (void *)ptr, (void *)&c, s);
		patched += s;
	}
	mutex_unlock(&text_mutex);
	return addr;
}

static void do_sync_core(void *info)
{
	sync_core();
}

void text_poke_sync(void)
{
	on_each_cpu(do_sync_core, NULL, 1);
}

 
struct text_poke_loc {
	 
	s32 rel_addr;
	s32 disp;
	u8 len;
	u8 opcode;
	const u8 text[POKE_MAX_OPCODE_SIZE];
	 
	u8 old;
};

struct bp_patching_desc {
	struct text_poke_loc *vec;
	int nr_entries;
	atomic_t refs;
};

static struct bp_patching_desc bp_desc;

static __always_inline
struct bp_patching_desc *try_get_desc(void)
{
	struct bp_patching_desc *desc = &bp_desc;

	if (!raw_atomic_inc_not_zero(&desc->refs))
		return NULL;

	return desc;
}

static __always_inline void put_desc(void)
{
	struct bp_patching_desc *desc = &bp_desc;

	smp_mb__before_atomic();
	raw_atomic_dec(&desc->refs);
}

static __always_inline void *text_poke_addr(struct text_poke_loc *tp)
{
	return _stext + tp->rel_addr;
}

static __always_inline int patch_cmp(const void *key, const void *elt)
{
	struct text_poke_loc *tp = (struct text_poke_loc *) elt;

	if (key < text_poke_addr(tp))
		return -1;
	if (key > text_poke_addr(tp))
		return 1;
	return 0;
}

noinstr int poke_int3_handler(struct pt_regs *regs)
{
	struct bp_patching_desc *desc;
	struct text_poke_loc *tp;
	int ret = 0;
	void *ip;

	if (user_mode(regs))
		return 0;

	 
	smp_rmb();

	desc = try_get_desc();
	if (!desc)
		return 0;

	 
	ip = (void *) regs->ip - INT3_INSN_SIZE;

	 
	if (unlikely(desc->nr_entries > 1)) {
		tp = __inline_bsearch(ip, desc->vec, desc->nr_entries,
				      sizeof(struct text_poke_loc),
				      patch_cmp);
		if (!tp)
			goto out_put;
	} else {
		tp = desc->vec;
		if (text_poke_addr(tp) != ip)
			goto out_put;
	}

	ip += tp->len;

	switch (tp->opcode) {
	case INT3_INSN_OPCODE:
		 
		goto out_put;

	case RET_INSN_OPCODE:
		int3_emulate_ret(regs);
		break;

	case CALL_INSN_OPCODE:
		int3_emulate_call(regs, (long)ip + tp->disp);
		break;

	case JMP32_INSN_OPCODE:
	case JMP8_INSN_OPCODE:
		int3_emulate_jmp(regs, (long)ip + tp->disp);
		break;

	case 0x70 ... 0x7f:  
		int3_emulate_jcc(regs, tp->opcode & 0xf, (long)ip, tp->disp);
		break;

	default:
		BUG();
	}

	ret = 1;

out_put:
	put_desc();
	return ret;
}

#define TP_VEC_MAX (PAGE_SIZE / sizeof(struct text_poke_loc))
static struct text_poke_loc tp_vec[TP_VEC_MAX];
static int tp_vec_nr;

 
static void text_poke_bp_batch(struct text_poke_loc *tp, unsigned int nr_entries)
{
	unsigned char int3 = INT3_INSN_OPCODE;
	unsigned int i;
	int do_sync;

	lockdep_assert_held(&text_mutex);

	bp_desc.vec = tp;
	bp_desc.nr_entries = nr_entries;

	 
	atomic_set_release(&bp_desc.refs, 1);

	 
	cond_resched();

	 
	smp_wmb();

	 
	for (i = 0; i < nr_entries; i++) {
		tp[i].old = *(u8 *)text_poke_addr(&tp[i]);
		text_poke(text_poke_addr(&tp[i]), &int3, INT3_INSN_SIZE);
	}

	text_poke_sync();

	 
	for (do_sync = 0, i = 0; i < nr_entries; i++) {
		u8 old[POKE_MAX_OPCODE_SIZE+1] = { tp[i].old, };
		u8 _new[POKE_MAX_OPCODE_SIZE+1];
		const u8 *new = tp[i].text;
		int len = tp[i].len;

		if (len - INT3_INSN_SIZE > 0) {
			memcpy(old + INT3_INSN_SIZE,
			       text_poke_addr(&tp[i]) + INT3_INSN_SIZE,
			       len - INT3_INSN_SIZE);

			if (len == 6) {
				_new[0] = 0x0f;
				memcpy(_new + 1, new, 5);
				new = _new;
			}

			text_poke(text_poke_addr(&tp[i]) + INT3_INSN_SIZE,
				  new + INT3_INSN_SIZE,
				  len - INT3_INSN_SIZE);

			do_sync++;
		}

		 
		perf_event_text_poke(text_poke_addr(&tp[i]), old, len, new, len);
	}

	if (do_sync) {
		 
		text_poke_sync();
	}

	 
	for (do_sync = 0, i = 0; i < nr_entries; i++) {
		u8 byte = tp[i].text[0];

		if (tp[i].len == 6)
			byte = 0x0f;

		if (byte == INT3_INSN_OPCODE)
			continue;

		text_poke(text_poke_addr(&tp[i]), &byte, INT3_INSN_SIZE);
		do_sync++;
	}

	if (do_sync)
		text_poke_sync();

	 
	if (!atomic_dec_and_test(&bp_desc.refs))
		atomic_cond_read_acquire(&bp_desc.refs, !VAL);
}

static void text_poke_loc_init(struct text_poke_loc *tp, void *addr,
			       const void *opcode, size_t len, const void *emulate)
{
	struct insn insn;
	int ret, i = 0;

	if (len == 6)
		i = 1;
	memcpy((void *)tp->text, opcode+i, len-i);
	if (!emulate)
		emulate = opcode;

	ret = insn_decode_kernel(&insn, emulate);
	BUG_ON(ret < 0);

	tp->rel_addr = addr - (void *)_stext;
	tp->len = len;
	tp->opcode = insn.opcode.bytes[0];

	if (is_jcc32(&insn)) {
		 
		tp->opcode = insn.opcode.bytes[1] - 0x10;
	}

	switch (tp->opcode) {
	case RET_INSN_OPCODE:
	case JMP32_INSN_OPCODE:
	case JMP8_INSN_OPCODE:
		 
		for (i = insn.length; i < len; i++)
			BUG_ON(tp->text[i] != INT3_INSN_OPCODE);
		break;

	default:
		BUG_ON(len != insn.length);
	}

	switch (tp->opcode) {
	case INT3_INSN_OPCODE:
	case RET_INSN_OPCODE:
		break;

	case CALL_INSN_OPCODE:
	case JMP32_INSN_OPCODE:
	case JMP8_INSN_OPCODE:
	case 0x70 ... 0x7f:  
		tp->disp = insn.immediate.value;
		break;

	default:  
		switch (len) {
		case 2:  
			BUG_ON(memcmp(emulate, x86_nops[len], len));
			tp->opcode = JMP8_INSN_OPCODE;
			tp->disp = 0;
			break;

		case 5:  
			BUG_ON(memcmp(emulate, x86_nops[len], len));
			tp->opcode = JMP32_INSN_OPCODE;
			tp->disp = 0;
			break;

		default:  
			BUG();
		}
		break;
	}
}

 
static bool tp_order_fail(void *addr)
{
	struct text_poke_loc *tp;

	if (!tp_vec_nr)
		return false;

	if (!addr)  
		return true;

	tp = &tp_vec[tp_vec_nr - 1];
	if ((unsigned long)text_poke_addr(tp) > (unsigned long)addr)
		return true;

	return false;
}

static void text_poke_flush(void *addr)
{
	if (tp_vec_nr == TP_VEC_MAX || tp_order_fail(addr)) {
		text_poke_bp_batch(tp_vec, tp_vec_nr);
		tp_vec_nr = 0;
	}
}

void text_poke_finish(void)
{
	text_poke_flush(NULL);
}

void __ref text_poke_queue(void *addr, const void *opcode, size_t len, const void *emulate)
{
	struct text_poke_loc *tp;

	text_poke_flush(addr);

	tp = &tp_vec[tp_vec_nr++];
	text_poke_loc_init(tp, addr, opcode, len, emulate);
}

 
void __ref text_poke_bp(void *addr, const void *opcode, size_t len, const void *emulate)
{
	struct text_poke_loc tp;

	text_poke_loc_init(&tp, addr, opcode, len, emulate);
	text_poke_bp_batch(&tp, 1);
}
