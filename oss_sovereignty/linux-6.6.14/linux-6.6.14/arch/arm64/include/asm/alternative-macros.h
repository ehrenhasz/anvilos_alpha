#ifndef __ASM_ALTERNATIVE_MACROS_H
#define __ASM_ALTERNATIVE_MACROS_H
#include <linux/const.h>
#include <vdso/bits.h>
#include <asm/cpucaps.h>
#include <asm/insn-def.h>
#define ARM64_CB_SHIFT	15
#define ARM64_CB_BIT	BIT(ARM64_CB_SHIFT)
#if ARM64_NCAPS >= ARM64_CB_BIT
#error "cpucaps have overflown ARM64_CB_BIT"
#endif
#ifndef __ASSEMBLY__
#include <linux/stringify.h>
#define ALTINSTR_ENTRY(cpucap)					              \
	" .word 661b - .\n"				  \
	" .word 663f - .\n"				  \
	" .hword " __stringify(cpucap) "\n"		  \
	" .byte 662b-661b\n"				  \
	" .byte 664f-663f\n"				 
#define ALTINSTR_ENTRY_CB(cpucap, cb)					      \
	" .word 661b - .\n"				  \
	" .word " __stringify(cb) "- .\n"		  \
	" .hword " __stringify(cpucap) "\n"		  \
	" .byte 662b-661b\n"				  \
	" .byte 664f-663f\n"				 
#define __ALTERNATIVE_CFG(oldinstr, newinstr, cpucap, cfg_enabled)	\
	".if "__stringify(cfg_enabled)" == 1\n"				\
	"661:\n\t"							\
	oldinstr "\n"							\
	"662:\n"							\
	".pushsection .altinstructions,\"a\"\n"				\
	ALTINSTR_ENTRY(cpucap)						\
	".popsection\n"							\
	".subsection 1\n"						\
	"663:\n\t"							\
	newinstr "\n"							\
	"664:\n\t"							\
	".org	. - (664b-663b) + (662b-661b)\n\t"			\
	".org	. - (662b-661b) + (664b-663b)\n\t"			\
	".previous\n"							\
	".endif\n"
#define __ALTERNATIVE_CFG_CB(oldinstr, cpucap, cfg_enabled, cb)	\
	".if "__stringify(cfg_enabled)" == 1\n"				\
	"661:\n\t"							\
	oldinstr "\n"							\
	"662:\n"							\
	".pushsection .altinstructions,\"a\"\n"				\
	ALTINSTR_ENTRY_CB(cpucap, cb)					\
	".popsection\n"							\
	"663:\n\t"							\
	"664:\n\t"							\
	".endif\n"
#define _ALTERNATIVE_CFG(oldinstr, newinstr, cpucap, cfg, ...)	\
	__ALTERNATIVE_CFG(oldinstr, newinstr, cpucap, IS_ENABLED(cfg))
#define ALTERNATIVE_CB(oldinstr, cpucap, cb) \
	__ALTERNATIVE_CFG_CB(oldinstr, (1 << ARM64_CB_SHIFT) | (cpucap), 1, cb)
#else
#include <asm/assembler.h>
.macro altinstruction_entry orig_offset alt_offset cpucap orig_len alt_len
	.word \orig_offset - .
	.word \alt_offset - .
	.hword (\cpucap)
	.byte \orig_len
	.byte \alt_len
.endm
.macro alternative_insn insn1, insn2, cap, enable = 1
	.if \enable
661:	\insn1
662:	.pushsection .altinstructions, "a"
	altinstruction_entry 661b, 663f, \cap, 662b-661b, 664f-663f
	.popsection
	.subsection 1
663:	\insn2
664:	.org	. - (664b-663b) + (662b-661b)
	.org	. - (662b-661b) + (664b-663b)
	.previous
	.endif
.endm
.macro alternative_if_not cap
	.set .Lasm_alt_mode, 0
	.pushsection .altinstructions, "a"
	altinstruction_entry 661f, 663f, \cap, 662f-661f, 664f-663f
	.popsection
661:
.endm
.macro alternative_if cap
	.set .Lasm_alt_mode, 1
	.pushsection .altinstructions, "a"
	altinstruction_entry 663f, 661f, \cap, 664f-663f, 662f-661f
	.popsection
	.subsection 1
	.align 2	 
661:
.endm
.macro alternative_cb cap, cb
	.set .Lasm_alt_mode, 0
	.pushsection .altinstructions, "a"
	altinstruction_entry 661f, \cb, (1 << ARM64_CB_SHIFT) | \cap, 662f-661f, 0
	.popsection
661:
.endm
.macro alternative_else
662:
	.if .Lasm_alt_mode==0
	.subsection 1
	.else
	.previous
	.endif
663:
.endm
.macro alternative_endif
664:
	.org	. - (664b-663b) + (662b-661b)
	.org	. - (662b-661b) + (664b-663b)
	.if .Lasm_alt_mode==0
	.previous
	.endif
.endm
.macro alternative_cb_end
662:
.endm
.macro alternative_else_nop_endif
alternative_else
	nops	(662b-661b) / AARCH64_INSN_SIZE
alternative_endif
.endm
#define _ALTERNATIVE_CFG(insn1, insn2, cap, cfg, ...)	\
	alternative_insn insn1, insn2, cap, IS_ENABLED(cfg)
#endif   
#define ALTERNATIVE(oldinstr, newinstr, ...)   \
	_ALTERNATIVE_CFG(oldinstr, newinstr, __VA_ARGS__, 1)
#ifndef __ASSEMBLY__
#include <linux/types.h>
static __always_inline bool
alternative_has_cap_likely(const unsigned long cpucap)
{
	compiletime_assert(cpucap < ARM64_NCAPS,
			   "cpucap must be < ARM64_NCAPS");
	asm_volatile_goto(
	ALTERNATIVE_CB("b	%l[l_no]", %[cpucap], alt_cb_patch_nops)
	:
	: [cpucap] "i" (cpucap)
	:
	: l_no);
	return true;
l_no:
	return false;
}
static __always_inline bool
alternative_has_cap_unlikely(const unsigned long cpucap)
{
	compiletime_assert(cpucap < ARM64_NCAPS,
			   "cpucap must be < ARM64_NCAPS");
	asm_volatile_goto(
	ALTERNATIVE("nop", "b	%l[l_yes]", %[cpucap])
	:
	: [cpucap] "i" (cpucap)
	:
	: l_yes);
	return false;
l_yes:
	return true;
}
#endif  
#endif  
