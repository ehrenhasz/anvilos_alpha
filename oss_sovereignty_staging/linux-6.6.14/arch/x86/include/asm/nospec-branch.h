 

#ifndef _ASM_X86_NOSPEC_BRANCH_H_
#define _ASM_X86_NOSPEC_BRANCH_H_

#include <linux/static_key.h>
#include <linux/objtool.h>
#include <linux/linkage.h>

#include <asm/alternative.h>
#include <asm/cpufeatures.h>
#include <asm/msr-index.h>
#include <asm/unwind_hints.h>
#include <asm/percpu.h>
#include <asm/current.h>

 
#define RET_DEPTH_SHIFT			5
#define RSB_RET_STUFF_LOOPS		16
#define RET_DEPTH_INIT			0x8000000000000000ULL
#define RET_DEPTH_INIT_FROM_CALL	0xfc00000000000000ULL
#define RET_DEPTH_CREDIT		0xffffffffffffffffULL

#ifdef CONFIG_CALL_THUNKS_DEBUG
# define CALL_THUNKS_DEBUG_INC_CALLS				\
	incq	%gs:__x86_call_count;
# define CALL_THUNKS_DEBUG_INC_RETS				\
	incq	%gs:__x86_ret_count;
# define CALL_THUNKS_DEBUG_INC_STUFFS				\
	incq	%gs:__x86_stuffs_count;
# define CALL_THUNKS_DEBUG_INC_CTXSW				\
	incq	%gs:__x86_ctxsw_count;
#else
# define CALL_THUNKS_DEBUG_INC_CALLS
# define CALL_THUNKS_DEBUG_INC_RETS
# define CALL_THUNKS_DEBUG_INC_STUFFS
# define CALL_THUNKS_DEBUG_INC_CTXSW
#endif

#if defined(CONFIG_CALL_DEPTH_TRACKING) && !defined(COMPILE_OFFSETS)

#include <asm/asm-offsets.h>

#define CREDIT_CALL_DEPTH					\
	movq	$-1, PER_CPU_VAR(pcpu_hot + X86_call_depth);

#define ASM_CREDIT_CALL_DEPTH					\
	movq	$-1, PER_CPU_VAR(pcpu_hot + X86_call_depth);

#define RESET_CALL_DEPTH					\
	xor	%eax, %eax;					\
	bts	$63, %rax;					\
	movq	%rax, PER_CPU_VAR(pcpu_hot + X86_call_depth);

#define RESET_CALL_DEPTH_FROM_CALL				\
	movb	$0xfc, %al;					\
	shl	$56, %rax;					\
	movq	%rax, PER_CPU_VAR(pcpu_hot + X86_call_depth);	\
	CALL_THUNKS_DEBUG_INC_CALLS

#define INCREMENT_CALL_DEPTH					\
	sarq	$5, %gs:pcpu_hot + X86_call_depth;		\
	CALL_THUNKS_DEBUG_INC_CALLS

#define ASM_INCREMENT_CALL_DEPTH				\
	sarq	$5, PER_CPU_VAR(pcpu_hot + X86_call_depth);	\
	CALL_THUNKS_DEBUG_INC_CALLS

#else
#define CREDIT_CALL_DEPTH
#define ASM_CREDIT_CALL_DEPTH
#define RESET_CALL_DEPTH
#define INCREMENT_CALL_DEPTH
#define ASM_INCREMENT_CALL_DEPTH
#define RESET_CALL_DEPTH_FROM_CALL
#endif

 

#define RETPOLINE_THUNK_SIZE	32
#define RSB_CLEAR_LOOPS		32	 

 
#define __FILL_RETURN_SLOT			\
	ANNOTATE_INTRA_FUNCTION_CALL;		\
	call	772f;				\
	int3;					\
772:

 
#ifdef CONFIG_X86_64
#define __FILL_RETURN_BUFFER(reg, nr)			\
	mov	$(nr/2), reg;				\
771:							\
	__FILL_RETURN_SLOT				\
	__FILL_RETURN_SLOT				\
	add	$(BITS_PER_LONG/8) * 2, %_ASM_SP;	\
	dec	reg;					\
	jnz	771b;					\
	 		\
	lfence;						\
	ASM_CREDIT_CALL_DEPTH				\
	CALL_THUNKS_DEBUG_INC_CTXSW
#else
 
#define __FILL_RETURN_BUFFER(reg, nr)			\
	.rept nr;					\
	__FILL_RETURN_SLOT;				\
	.endr;						\
	add	$(BITS_PER_LONG/8) * nr, %_ASM_SP;
#endif

 
#define __FILL_ONE_RETURN				\
	__FILL_RETURN_SLOT				\
	add	$(BITS_PER_LONG/8), %_ASM_SP;		\
	lfence;

#ifdef __ASSEMBLY__

 
.macro ANNOTATE_RETPOLINE_SAFE
.Lhere_\@:
	.pushsection .discard.retpoline_safe
	.long .Lhere_\@
	.popsection
.endm

 
#define ANNOTATE_UNRET_SAFE ANNOTATE_RETPOLINE_SAFE

 
.macro VALIDATE_UNRET_END
#if defined(CONFIG_NOINSTR_VALIDATION) && \
	(defined(CONFIG_CPU_UNRET_ENTRY) || defined(CONFIG_CPU_SRSO))
	ANNOTATE_RETPOLINE_SAFE
	nop
#endif
.endm

 
.macro __CS_PREFIX reg:req
	.irp rs,r8,r9,r10,r11,r12,r13,r14,r15
	.ifc \reg,\rs
	.byte 0x2e
	.endif
	.endr
.endm

 
.macro JMP_NOSPEC reg:req
#ifdef CONFIG_RETPOLINE
	__CS_PREFIX \reg
	jmp	__x86_indirect_thunk_\reg
#else
	jmp	*%\reg
	int3
#endif
.endm

.macro CALL_NOSPEC reg:req
#ifdef CONFIG_RETPOLINE
	__CS_PREFIX \reg
	call	__x86_indirect_thunk_\reg
#else
	call	*%\reg
#endif
.endm

  
.macro FILL_RETURN_BUFFER reg:req nr:req ftr:req ftr2=ALT_NOT(X86_FEATURE_ALWAYS)
	ALTERNATIVE_2 "jmp .Lskip_rsb_\@", \
		__stringify(__FILL_RETURN_BUFFER(\reg,\nr)), \ftr, \
		__stringify(nop;nop;__FILL_ONE_RETURN), \ftr2

.Lskip_rsb_\@:
.endm

#if defined(CONFIG_CPU_UNRET_ENTRY) || defined(CONFIG_CPU_SRSO)
#define CALL_UNTRAIN_RET	"call entry_untrain_ret"
#else
#define CALL_UNTRAIN_RET	""
#endif

 
.macro UNTRAIN_RET
#if defined(CONFIG_CPU_UNRET_ENTRY) || defined(CONFIG_CPU_IBPB_ENTRY) || \
	defined(CONFIG_CALL_DEPTH_TRACKING) || defined(CONFIG_CPU_SRSO)
	VALIDATE_UNRET_END
	ALTERNATIVE_3 "",						\
		      CALL_UNTRAIN_RET, X86_FEATURE_UNRET,		\
		      "call entry_ibpb", X86_FEATURE_ENTRY_IBPB,	\
		      __stringify(RESET_CALL_DEPTH), X86_FEATURE_CALL_DEPTH
#endif
.endm

.macro UNTRAIN_RET_VM
#if defined(CONFIG_CPU_UNRET_ENTRY) || defined(CONFIG_CPU_IBPB_ENTRY) || \
	defined(CONFIG_CALL_DEPTH_TRACKING) || defined(CONFIG_CPU_SRSO)
	VALIDATE_UNRET_END
	ALTERNATIVE_3 "",						\
		      CALL_UNTRAIN_RET, X86_FEATURE_UNRET,		\
		      "call entry_ibpb", X86_FEATURE_IBPB_ON_VMEXIT,	\
		      __stringify(RESET_CALL_DEPTH), X86_FEATURE_CALL_DEPTH
#endif
.endm

.macro UNTRAIN_RET_FROM_CALL
#if defined(CONFIG_CPU_UNRET_ENTRY) || defined(CONFIG_CPU_IBPB_ENTRY) || \
	defined(CONFIG_CALL_DEPTH_TRACKING) || defined(CONFIG_CPU_SRSO)
	VALIDATE_UNRET_END
	ALTERNATIVE_3 "",						\
		      CALL_UNTRAIN_RET, X86_FEATURE_UNRET,		\
		      "call entry_ibpb", X86_FEATURE_ENTRY_IBPB,	\
		      __stringify(RESET_CALL_DEPTH_FROM_CALL), X86_FEATURE_CALL_DEPTH
#endif
.endm


.macro CALL_DEPTH_ACCOUNT
#ifdef CONFIG_CALL_DEPTH_TRACKING
	ALTERNATIVE "",							\
		    __stringify(ASM_INCREMENT_CALL_DEPTH), X86_FEATURE_CALL_DEPTH
#endif
.endm

#else  

#define ANNOTATE_RETPOLINE_SAFE					\
	"999:\n\t"						\
	".pushsection .discard.retpoline_safe\n\t"		\
	".long 999b\n\t"					\
	".popsection\n\t"

typedef u8 retpoline_thunk_t[RETPOLINE_THUNK_SIZE];
extern retpoline_thunk_t __x86_indirect_thunk_array[];
extern retpoline_thunk_t __x86_indirect_call_thunk_array[];
extern retpoline_thunk_t __x86_indirect_jump_thunk_array[];

#ifdef CONFIG_RETHUNK
extern void __x86_return_thunk(void);
#else
static inline void __x86_return_thunk(void) {}
#endif

extern void retbleed_return_thunk(void);
extern void srso_return_thunk(void);
extern void srso_alias_return_thunk(void);

extern void retbleed_untrain_ret(void);
extern void srso_untrain_ret(void);
extern void srso_alias_untrain_ret(void);

extern void entry_untrain_ret(void);
extern void entry_ibpb(void);

extern void (*x86_return_thunk)(void);

#ifdef CONFIG_CALL_DEPTH_TRACKING
extern void __x86_return_skl(void);

static inline void x86_set_skl_return_thunk(void)
{
	x86_return_thunk = &__x86_return_skl;
}

#define CALL_DEPTH_ACCOUNT					\
	ALTERNATIVE("",						\
		    __stringify(INCREMENT_CALL_DEPTH),		\
		    X86_FEATURE_CALL_DEPTH)

#ifdef CONFIG_CALL_THUNKS_DEBUG
DECLARE_PER_CPU(u64, __x86_call_count);
DECLARE_PER_CPU(u64, __x86_ret_count);
DECLARE_PER_CPU(u64, __x86_stuffs_count);
DECLARE_PER_CPU(u64, __x86_ctxsw_count);
#endif
#else
static inline void x86_set_skl_return_thunk(void) {}

#define CALL_DEPTH_ACCOUNT ""

#endif

#ifdef CONFIG_RETPOLINE

#define GEN(reg) \
	extern retpoline_thunk_t __x86_indirect_thunk_ ## reg;
#include <asm/GEN-for-each-reg.h>
#undef GEN

#define GEN(reg)						\
	extern retpoline_thunk_t __x86_indirect_call_thunk_ ## reg;
#include <asm/GEN-for-each-reg.h>
#undef GEN

#define GEN(reg)						\
	extern retpoline_thunk_t __x86_indirect_jump_thunk_ ## reg;
#include <asm/GEN-for-each-reg.h>
#undef GEN

#ifdef CONFIG_X86_64

 
# define CALL_NOSPEC						\
	ALTERNATIVE_2(						\
	ANNOTATE_RETPOLINE_SAFE					\
	"call *%[thunk_target]\n",				\
	"call __x86_indirect_thunk_%V[thunk_target]\n",		\
	X86_FEATURE_RETPOLINE,					\
	"lfence;\n"						\
	ANNOTATE_RETPOLINE_SAFE					\
	"call *%[thunk_target]\n",				\
	X86_FEATURE_RETPOLINE_LFENCE)

# define THUNK_TARGET(addr) [thunk_target] "r" (addr)

#else  
 
# define CALL_NOSPEC						\
	ALTERNATIVE_2(						\
	ANNOTATE_RETPOLINE_SAFE					\
	"call *%[thunk_target]\n",				\
	"       jmp    904f;\n"					\
	"       .align 16\n"					\
	"901:	call   903f;\n"					\
	"902:	pause;\n"					\
	"    	lfence;\n"					\
	"       jmp    902b;\n"					\
	"       .align 16\n"					\
	"903:	lea    4(%%esp), %%esp;\n"			\
	"       pushl  %[thunk_target];\n"			\
	"       ret;\n"						\
	"       .align 16\n"					\
	"904:	call   901b;\n",				\
	X86_FEATURE_RETPOLINE,					\
	"lfence;\n"						\
	ANNOTATE_RETPOLINE_SAFE					\
	"call *%[thunk_target]\n",				\
	X86_FEATURE_RETPOLINE_LFENCE)

# define THUNK_TARGET(addr) [thunk_target] "rm" (addr)
#endif
#else  
# define CALL_NOSPEC "call *%[thunk_target]\n"
# define THUNK_TARGET(addr) [thunk_target] "rm" (addr)
#endif

 
enum spectre_v2_mitigation {
	SPECTRE_V2_NONE,
	SPECTRE_V2_RETPOLINE,
	SPECTRE_V2_LFENCE,
	SPECTRE_V2_EIBRS,
	SPECTRE_V2_EIBRS_RETPOLINE,
	SPECTRE_V2_EIBRS_LFENCE,
	SPECTRE_V2_IBRS,
};

 
enum spectre_v2_user_mitigation {
	SPECTRE_V2_USER_NONE,
	SPECTRE_V2_USER_STRICT,
	SPECTRE_V2_USER_STRICT_PREFERRED,
	SPECTRE_V2_USER_PRCTL,
	SPECTRE_V2_USER_SECCOMP,
};

 
enum ssb_mitigation {
	SPEC_STORE_BYPASS_NONE,
	SPEC_STORE_BYPASS_DISABLE,
	SPEC_STORE_BYPASS_PRCTL,
	SPEC_STORE_BYPASS_SECCOMP,
};

static __always_inline
void alternative_msr_write(unsigned int msr, u64 val, unsigned int feature)
{
	asm volatile(ALTERNATIVE("", "wrmsr", %c[feature])
		: : "c" (msr),
		    "a" ((u32)val),
		    "d" ((u32)(val >> 32)),
		    [feature] "i" (feature)
		: "memory");
}

extern u64 x86_pred_cmd;

static inline void indirect_branch_prediction_barrier(void)
{
	alternative_msr_write(MSR_IA32_PRED_CMD, x86_pred_cmd, X86_FEATURE_USE_IBPB);
}

 
extern u64 x86_spec_ctrl_base;
DECLARE_PER_CPU(u64, x86_spec_ctrl_current);
extern void update_spec_ctrl_cond(u64 val);
extern u64 spec_ctrl_current(void);

 
#define firmware_restrict_branch_speculation_start()			\
do {									\
	preempt_disable();						\
	alternative_msr_write(MSR_IA32_SPEC_CTRL,			\
			      spec_ctrl_current() | SPEC_CTRL_IBRS,	\
			      X86_FEATURE_USE_IBRS_FW);			\
	alternative_msr_write(MSR_IA32_PRED_CMD, PRED_CMD_IBPB,		\
			      X86_FEATURE_USE_IBPB_FW);			\
} while (0)

#define firmware_restrict_branch_speculation_end()			\
do {									\
	alternative_msr_write(MSR_IA32_SPEC_CTRL,			\
			      spec_ctrl_current(),			\
			      X86_FEATURE_USE_IBRS_FW);			\
	preempt_enable();						\
} while (0)

DECLARE_STATIC_KEY_FALSE(switch_to_cond_stibp);
DECLARE_STATIC_KEY_FALSE(switch_mm_cond_ibpb);
DECLARE_STATIC_KEY_FALSE(switch_mm_always_ibpb);

DECLARE_STATIC_KEY_FALSE(mds_user_clear);
DECLARE_STATIC_KEY_FALSE(mds_idle_clear);

DECLARE_STATIC_KEY_FALSE(switch_mm_cond_l1d_flush);

DECLARE_STATIC_KEY_FALSE(mmio_stale_data_clear);

#include <asm/segment.h>

 
static __always_inline void mds_clear_cpu_buffers(void)
{
	static const u16 ds = __KERNEL_DS;

	 
	asm volatile("verw %[ds]" : : [ds] "m" (ds) : "cc");
}

 
static __always_inline void mds_user_clear_cpu_buffers(void)
{
	if (static_branch_likely(&mds_user_clear))
		mds_clear_cpu_buffers();
}

 
static __always_inline void mds_idle_clear_cpu_buffers(void)
{
	if (static_branch_likely(&mds_idle_clear))
		mds_clear_cpu_buffers();
}

#endif  

#endif  
