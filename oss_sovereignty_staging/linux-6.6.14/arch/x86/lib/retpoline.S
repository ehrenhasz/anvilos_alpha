 

#include <linux/stringify.h>
#include <linux/linkage.h>
#include <asm/dwarf2.h>
#include <asm/cpufeatures.h>
#include <asm/alternative.h>
#include <asm/asm-offsets.h>
#include <asm/export.h>
#include <asm/nospec-branch.h>
#include <asm/unwind_hints.h>
#include <asm/percpu.h>
#include <asm/frame.h>
#include <asm/nops.h>

	.section .text..__x86.indirect_thunk


.macro POLINE reg
	ANNOTATE_INTRA_FUNCTION_CALL
	call    .Ldo_rop_\@
	int3
.Ldo_rop_\@:
	mov     %\reg, (%_ASM_SP)
	UNWIND_HINT_FUNC
.endm

.macro RETPOLINE reg
	POLINE \reg
	RET
.endm

.macro THUNK reg

	.align RETPOLINE_THUNK_SIZE
SYM_INNER_LABEL(__x86_indirect_thunk_\reg, SYM_L_GLOBAL)
	UNWIND_HINT_UNDEFINED
	ANNOTATE_NOENDBR

	ALTERNATIVE_2 __stringify(RETPOLINE \reg), \
		      __stringify(lfence; ANNOTATE_RETPOLINE_SAFE; jmp *%\reg; int3), X86_FEATURE_RETPOLINE_LFENCE, \
		      __stringify(ANNOTATE_RETPOLINE_SAFE; jmp *%\reg), ALT_NOT(X86_FEATURE_RETPOLINE)

.endm

 

#define __EXPORT_THUNK(sym)	_ASM_NOKPROBE(sym); EXPORT_SYMBOL(sym)

	.align RETPOLINE_THUNK_SIZE
SYM_CODE_START(__x86_indirect_thunk_array)

#define GEN(reg) THUNK reg
#include <asm/GEN-for-each-reg.h>
#undef GEN

	.align RETPOLINE_THUNK_SIZE
SYM_CODE_END(__x86_indirect_thunk_array)

#define GEN(reg) __EXPORT_THUNK(__x86_indirect_thunk_ ## reg)
#include <asm/GEN-for-each-reg.h>
#undef GEN

#ifdef CONFIG_CALL_DEPTH_TRACKING
.macro CALL_THUNK reg
	.align RETPOLINE_THUNK_SIZE

SYM_INNER_LABEL(__x86_indirect_call_thunk_\reg, SYM_L_GLOBAL)
	UNWIND_HINT_UNDEFINED
	ANNOTATE_NOENDBR

	CALL_DEPTH_ACCOUNT
	POLINE \reg
	ANNOTATE_UNRET_SAFE
	ret
	int3
.endm

	.align RETPOLINE_THUNK_SIZE
SYM_CODE_START(__x86_indirect_call_thunk_array)

#define GEN(reg) CALL_THUNK reg
#include <asm/GEN-for-each-reg.h>
#undef GEN

	.align RETPOLINE_THUNK_SIZE
SYM_CODE_END(__x86_indirect_call_thunk_array)

#define GEN(reg) __EXPORT_THUNK(__x86_indirect_call_thunk_ ## reg)
#include <asm/GEN-for-each-reg.h>
#undef GEN

.macro JUMP_THUNK reg
	.align RETPOLINE_THUNK_SIZE

SYM_INNER_LABEL(__x86_indirect_jump_thunk_\reg, SYM_L_GLOBAL)
	UNWIND_HINT_UNDEFINED
	ANNOTATE_NOENDBR
	POLINE \reg
	ANNOTATE_UNRET_SAFE
	ret
	int3
.endm

	.align RETPOLINE_THUNK_SIZE
SYM_CODE_START(__x86_indirect_jump_thunk_array)

#define GEN(reg) JUMP_THUNK reg
#include <asm/GEN-for-each-reg.h>
#undef GEN

	.align RETPOLINE_THUNK_SIZE
SYM_CODE_END(__x86_indirect_jump_thunk_array)

#define GEN(reg) __EXPORT_THUNK(__x86_indirect_jump_thunk_ ## reg)
#include <asm/GEN-for-each-reg.h>
#undef GEN
#endif
 
#ifdef CONFIG_RETHUNK

 
#ifdef CONFIG_CPU_SRSO
	.section .text..__x86.rethunk_untrain

SYM_START(srso_alias_untrain_ret, SYM_L_GLOBAL, SYM_A_NONE)
	UNWIND_HINT_FUNC
	ANNOTATE_NOENDBR
	ASM_NOP2
	lfence
	jmp srso_alias_return_thunk
SYM_FUNC_END(srso_alias_untrain_ret)
__EXPORT_THUNK(srso_alias_untrain_ret)

	.section .text..__x86.rethunk_safe
#else
 
SYM_START(srso_alias_untrain_ret, SYM_L_GLOBAL, SYM_A_NONE)
	ANNOTATE_UNRET_SAFE
	ret
	int3
SYM_FUNC_END(srso_alias_untrain_ret)
#endif

SYM_START(srso_alias_safe_ret, SYM_L_GLOBAL, SYM_A_NONE)
	lea 8(%_ASM_SP), %_ASM_SP
	UNWIND_HINT_FUNC
	ANNOTATE_UNRET_SAFE
	ret
	int3
SYM_FUNC_END(srso_alias_safe_ret)

	.section .text..__x86.return_thunk

SYM_CODE_START(srso_alias_return_thunk)
	UNWIND_HINT_FUNC
	ANNOTATE_NOENDBR
	call srso_alias_safe_ret
	ud2
SYM_CODE_END(srso_alias_return_thunk)

 

 
	.align 64
	.skip 64 - (retbleed_return_thunk - retbleed_untrain_ret), 0xcc
SYM_START(retbleed_untrain_ret, SYM_L_GLOBAL, SYM_A_NONE)
	ANNOTATE_NOENDBR
	 
	.byte	0xf6

	 
SYM_INNER_LABEL(retbleed_return_thunk, SYM_L_GLOBAL)
	ret
	int3
SYM_CODE_END(retbleed_return_thunk)

	 
	lfence

	 
	jmp retbleed_return_thunk
	int3
SYM_FUNC_END(retbleed_untrain_ret)
__EXPORT_THUNK(retbleed_untrain_ret)

 
	.align 64
	.skip 64 - (srso_safe_ret - srso_untrain_ret), 0xcc
SYM_START(srso_untrain_ret, SYM_L_GLOBAL, SYM_A_NONE)
	ANNOTATE_NOENDBR
	.byte 0x48, 0xb8

 
SYM_INNER_LABEL(srso_safe_ret, SYM_L_GLOBAL)
	lea 8(%_ASM_SP), %_ASM_SP
	ret
	int3
	int3
	 
	lfence
	call srso_safe_ret
	ud2
SYM_CODE_END(srso_safe_ret)
SYM_FUNC_END(srso_untrain_ret)
__EXPORT_THUNK(srso_untrain_ret)

SYM_CODE_START(srso_return_thunk)
	UNWIND_HINT_FUNC
	ANNOTATE_NOENDBR
	call srso_safe_ret
	ud2
SYM_CODE_END(srso_return_thunk)

SYM_FUNC_START(entry_untrain_ret)
	ALTERNATIVE_2 "jmp retbleed_untrain_ret", \
		      "jmp srso_untrain_ret", X86_FEATURE_SRSO, \
		      "jmp srso_alias_untrain_ret", X86_FEATURE_SRSO_ALIAS
SYM_FUNC_END(entry_untrain_ret)
__EXPORT_THUNK(entry_untrain_ret)

SYM_CODE_START(__x86_return_thunk)
	UNWIND_HINT_FUNC
	ANNOTATE_NOENDBR
	ANNOTATE_UNRET_SAFE
	ret
	int3
SYM_CODE_END(__x86_return_thunk)
EXPORT_SYMBOL(__x86_return_thunk)

#endif  

#ifdef CONFIG_CALL_DEPTH_TRACKING

	.align 64
SYM_FUNC_START(__x86_return_skl)
	ANNOTATE_NOENDBR
	 
	CALL_THUNKS_DEBUG_INC_RETS
	shlq	$5, PER_CPU_VAR(pcpu_hot + X86_call_depth)
	jz	1f
	ANNOTATE_UNRET_SAFE
	ret
	int3
1:
	CALL_THUNKS_DEBUG_INC_STUFFS
	.rept	16
	ANNOTATE_INTRA_FUNCTION_CALL
	call	2f
	int3
2:
	.endr
	add	$(8*16), %rsp

	CREDIT_CALL_DEPTH

	ANNOTATE_UNRET_SAFE
	ret
	int3
SYM_FUNC_END(__x86_return_skl)

#endif  
