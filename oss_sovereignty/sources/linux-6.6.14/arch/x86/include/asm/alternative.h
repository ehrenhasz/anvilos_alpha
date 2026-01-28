
#ifndef _ASM_X86_ALTERNATIVE_H
#define _ASM_X86_ALTERNATIVE_H

#include <linux/types.h>
#include <linux/stringify.h>
#include <asm/asm.h>

#define ALT_FLAGS_SHIFT		16

#define ALT_FLAG_NOT		(1 << 0)
#define ALT_NOT(feature)	((ALT_FLAG_NOT << ALT_FLAGS_SHIFT) | (feature))

#ifndef __ASSEMBLY__

#include <linux/stddef.h>



#ifdef CONFIG_SMP
#define LOCK_PREFIX_HERE \
		".pushsection .smp_locks,\"a\"\n"	\
		".balign 4\n"				\
		".long 671f - .\n" 		\
		".popsection\n"				\
		"671:"

#define LOCK_PREFIX LOCK_PREFIX_HERE "\n\tlock; "

#else 
#define LOCK_PREFIX_HERE ""
#define LOCK_PREFIX ""
#endif


#define ANNOTATE_IGNORE_ALTERNATIVE				\
	"999:\n\t"						\
	".pushsection .discard.ignore_alts\n\t"			\
	".long 999b\n\t"					\
	".popsection\n\t"


struct alt_instr {
	s32 instr_offset;	
	s32 repl_offset;	

	union {
		struct {
			u32 cpuid: 16;	
			u32 flags: 16;	
		};
		u32 ft_flags;
	};

	u8  instrlen;		
	u8  replacementlen;	
} __packed;


extern int alternatives_patched;

extern void alternative_instructions(void);
extern void apply_alternatives(struct alt_instr *start, struct alt_instr *end);
extern void apply_retpolines(s32 *start, s32 *end);
extern void apply_returns(s32 *start, s32 *end);
extern void apply_seal_endbr(s32 *start, s32 *end);
extern void apply_fineibt(s32 *start_retpoline, s32 *end_retpoine,
			  s32 *start_cfi, s32 *end_cfi);

struct module;
struct paravirt_patch_site;

struct callthunk_sites {
	s32				*call_start, *call_end;
	struct paravirt_patch_site	*pv_start, *pv_end;
};

#ifdef CONFIG_CALL_THUNKS
extern void callthunks_patch_builtin_calls(void);
extern void callthunks_patch_module_calls(struct callthunk_sites *sites,
					  struct module *mod);
extern void *callthunks_translate_call_dest(void *dest);
extern int x86_call_depth_emit_accounting(u8 **pprog, void *func);
#else
static __always_inline void callthunks_patch_builtin_calls(void) {}
static __always_inline void
callthunks_patch_module_calls(struct callthunk_sites *sites,
			      struct module *mod) {}
static __always_inline void *callthunks_translate_call_dest(void *dest)
{
	return dest;
}
static __always_inline int x86_call_depth_emit_accounting(u8 **pprog,
							  void *func)
{
	return 0;
}
#endif

#ifdef CONFIG_SMP
extern void alternatives_smp_module_add(struct module *mod, char *name,
					void *locks, void *locks_end,
					void *text, void *text_end);
extern void alternatives_smp_module_del(struct module *mod);
extern void alternatives_enable_smp(void);
extern int alternatives_text_reserved(void *start, void *end);
extern bool skip_smp_alternatives;
#else
static inline void alternatives_smp_module_add(struct module *mod, char *name,
					       void *locks, void *locks_end,
					       void *text, void *text_end) {}
static inline void alternatives_smp_module_del(struct module *mod) {}
static inline void alternatives_enable_smp(void) {}
static inline int alternatives_text_reserved(void *start, void *end)
{
	return 0;
}
#endif	

#define b_replacement(num)	"664"#num
#define e_replacement(num)	"665"#num

#define alt_end_marker		"663"
#define alt_slen		"662b-661b"
#define alt_total_slen		alt_end_marker"b-661b"
#define alt_rlen(num)		e_replacement(num)"f-"b_replacement(num)"f"

#define OLDINSTR(oldinstr, num)						\
	"# ALT: oldnstr\n"						\
	"661:\n\t" oldinstr "\n662:\n"					\
	"# ALT: padding\n"						\
	".skip -(((" alt_rlen(num) ")-(" alt_slen ")) > 0) * "		\
		"((" alt_rlen(num) ")-(" alt_slen ")),0x90\n"		\
	alt_end_marker ":\n"


#define alt_max_short(a, b)	"((" a ") ^ (((" a ") ^ (" b ")) & -(-((" a ") < (" b ")))))"


#define OLDINSTR_2(oldinstr, num1, num2) \
	"# ALT: oldinstr2\n"									\
	"661:\n\t" oldinstr "\n662:\n"								\
	"# ALT: padding2\n"									\
	".skip -((" alt_max_short(alt_rlen(num1), alt_rlen(num2)) " - (" alt_slen ")) > 0) * "	\
		"(" alt_max_short(alt_rlen(num1), alt_rlen(num2)) " - (" alt_slen ")), 0x90\n"	\
	alt_end_marker ":\n"

#define OLDINSTR_3(oldinsn, n1, n2, n3)								\
	"# ALT: oldinstr3\n"									\
	"661:\n\t" oldinsn "\n662:\n"								\
	"# ALT: padding3\n"									\
	".skip -((" alt_max_short(alt_max_short(alt_rlen(n1), alt_rlen(n2)), alt_rlen(n3))	\
		" - (" alt_slen ")) > 0) * "							\
		"(" alt_max_short(alt_max_short(alt_rlen(n1), alt_rlen(n2)), alt_rlen(n3))	\
		" - (" alt_slen ")), 0x90\n"							\
	alt_end_marker ":\n"

#define ALTINSTR_ENTRY(ft_flags, num)					      \
	" .long 661b - .\n"				 \
	" .long " b_replacement(num)"f - .\n"		 \
	" .4byte " __stringify(ft_flags) "\n"		 \
	" .byte " alt_total_slen "\n"			 \
	" .byte " alt_rlen(num) "\n"			

#define ALTINSTR_REPLACEMENT(newinstr, num)			\
	"# ALT: replacement " #num "\n"						\
	b_replacement(num)":\n\t" newinstr "\n" e_replacement(num) ":\n"


#define ALTERNATIVE(oldinstr, newinstr, ft_flags)			\
	OLDINSTR(oldinstr, 1)						\
	".pushsection .altinstructions,\"a\"\n"				\
	ALTINSTR_ENTRY(ft_flags, 1)					\
	".popsection\n"							\
	".pushsection .altinstr_replacement, \"ax\"\n"			\
	ALTINSTR_REPLACEMENT(newinstr, 1)				\
	".popsection\n"

#define ALTERNATIVE_2(oldinstr, newinstr1, ft_flags1, newinstr2, ft_flags2) \
	OLDINSTR_2(oldinstr, 1, 2)					\
	".pushsection .altinstructions,\"a\"\n"				\
	ALTINSTR_ENTRY(ft_flags1, 1)					\
	ALTINSTR_ENTRY(ft_flags2, 2)					\
	".popsection\n"							\
	".pushsection .altinstr_replacement, \"ax\"\n"			\
	ALTINSTR_REPLACEMENT(newinstr1, 1)				\
	ALTINSTR_REPLACEMENT(newinstr2, 2)				\
	".popsection\n"


#define ALTERNATIVE_TERNARY(oldinstr, ft_flags, newinstr_yes, newinstr_no) \
	ALTERNATIVE_2(oldinstr, newinstr_no, X86_FEATURE_ALWAYS,	\
		      newinstr_yes, ft_flags)

#define ALTERNATIVE_3(oldinsn, newinsn1, ft_flags1, newinsn2, ft_flags2, \
			newinsn3, ft_flags3)				\
	OLDINSTR_3(oldinsn, 1, 2, 3)					\
	".pushsection .altinstructions,\"a\"\n"				\
	ALTINSTR_ENTRY(ft_flags1, 1)					\
	ALTINSTR_ENTRY(ft_flags2, 2)					\
	ALTINSTR_ENTRY(ft_flags3, 3)					\
	".popsection\n"							\
	".pushsection .altinstr_replacement, \"ax\"\n"			\
	ALTINSTR_REPLACEMENT(newinsn1, 1)				\
	ALTINSTR_REPLACEMENT(newinsn2, 2)				\
	ALTINSTR_REPLACEMENT(newinsn3, 3)				\
	".popsection\n"


#define alternative(oldinstr, newinstr, ft_flags)			\
	asm_inline volatile (ALTERNATIVE(oldinstr, newinstr, ft_flags) : : : "memory")

#define alternative_2(oldinstr, newinstr1, ft_flags1, newinstr2, ft_flags2) \
	asm_inline volatile(ALTERNATIVE_2(oldinstr, newinstr1, ft_flags1, newinstr2, ft_flags2) ::: "memory")

#define alternative_ternary(oldinstr, ft_flags, newinstr_yes, newinstr_no) \
	asm_inline volatile(ALTERNATIVE_TERNARY(oldinstr, ft_flags, newinstr_yes, newinstr_no) ::: "memory")


#define alternative_input(oldinstr, newinstr, ft_flags, input...)	\
	asm_inline volatile (ALTERNATIVE(oldinstr, newinstr, ft_flags)	\
		: : "i" (0), ## input)


#define alternative_input_2(oldinstr, newinstr1, ft_flags1, newinstr2,	     \
			   ft_flags2, input...)				     \
	asm_inline volatile(ALTERNATIVE_2(oldinstr, newinstr1, ft_flags1,     \
		newinstr2, ft_flags2)					     \
		: : "i" (0), ## input)


#define alternative_io(oldinstr, newinstr, ft_flags, output, input...)	\
	asm_inline volatile (ALTERNATIVE(oldinstr, newinstr, ft_flags)	\
		: output : "i" (0), ## input)


#define alternative_call(oldfunc, newfunc, ft_flags, output, input...)	\
	asm_inline volatile (ALTERNATIVE("call %P[old]", "call %P[new]", ft_flags) \
		: output : [old] "i" (oldfunc), [new] "i" (newfunc), ## input)


#define alternative_call_2(oldfunc, newfunc1, ft_flags1, newfunc2, ft_flags2,   \
			   output, input...)				      \
	asm_inline volatile (ALTERNATIVE_2("call %P[old]", "call %P[new1]", ft_flags1,\
		"call %P[new2]", ft_flags2)				      \
		: output, ASM_CALL_CONSTRAINT				      \
		: [old] "i" (oldfunc), [new1] "i" (newfunc1),		      \
		  [new2] "i" (newfunc2), ## input)


#define ASM_OUTPUT2(a...) a


#define ASM_NO_INPUT_CLOBBER(clbr...) "i" (0) : clbr

#else 

#ifdef CONFIG_SMP
	.macro LOCK_PREFIX
672:	lock
	.pushsection .smp_locks,"a"
	.balign 4
	.long 672b - .
	.popsection
	.endm
#else
	.macro LOCK_PREFIX
	.endm
#endif


.macro ANNOTATE_IGNORE_ALTERNATIVE
	.Lannotate_\@:
	.pushsection .discard.ignore_alts
	.long .Lannotate_\@
	.popsection
.endm


.macro altinstr_entry orig alt ft_flags orig_len alt_len
	.long \orig - .
	.long \alt - .
	.4byte \ft_flags
	.byte \orig_len
	.byte \alt_len
.endm


.macro ALTERNATIVE oldinstr, newinstr, ft_flags
140:
	\oldinstr
141:
	.skip -(((144f-143f)-(141b-140b)) > 0) * ((144f-143f)-(141b-140b)),0x90
142:

	.pushsection .altinstructions,"a"
	altinstr_entry 140b,143f,\ft_flags,142b-140b,144f-143f
	.popsection

	.pushsection .altinstr_replacement,"ax"
143:
	\newinstr
144:
	.popsection
.endm

#define old_len			141b-140b
#define new_len1		144f-143f
#define new_len2		145f-144f
#define new_len3		146f-145f


#define alt_max_2(a, b)		((a) ^ (((a) ^ (b)) & -(-((a) < (b)))))
#define alt_max_3(a, b, c)	(alt_max_2(alt_max_2(a, b), c))



.macro ALTERNATIVE_2 oldinstr, newinstr1, ft_flags1, newinstr2, ft_flags2
140:
	\oldinstr
141:
	.skip -((alt_max_2(new_len1, new_len2) - (old_len)) > 0) * \
		(alt_max_2(new_len1, new_len2) - (old_len)),0x90
142:

	.pushsection .altinstructions,"a"
	altinstr_entry 140b,143f,\ft_flags1,142b-140b,144f-143f
	altinstr_entry 140b,144f,\ft_flags2,142b-140b,145f-144f
	.popsection

	.pushsection .altinstr_replacement,"ax"
143:
	\newinstr1
144:
	\newinstr2
145:
	.popsection
.endm

.macro ALTERNATIVE_3 oldinstr, newinstr1, ft_flags1, newinstr2, ft_flags2, newinstr3, ft_flags3
140:
	\oldinstr
141:
	.skip -((alt_max_3(new_len1, new_len2, new_len3) - (old_len)) > 0) * \
		(alt_max_3(new_len1, new_len2, new_len3) - (old_len)),0x90
142:

	.pushsection .altinstructions,"a"
	altinstr_entry 140b,143f,\ft_flags1,142b-140b,144f-143f
	altinstr_entry 140b,144f,\ft_flags2,142b-140b,145f-144f
	altinstr_entry 140b,145f,\ft_flags3,142b-140b,146f-145f
	.popsection

	.pushsection .altinstr_replacement,"ax"
143:
	\newinstr1
144:
	\newinstr2
145:
	\newinstr3
146:
	.popsection
.endm


#define ALTERNATIVE_TERNARY(oldinstr, ft_flags, newinstr_yes, newinstr_no) \
	ALTERNATIVE_2 oldinstr, newinstr_no, X86_FEATURE_ALWAYS,	\
	newinstr_yes, ft_flags

#endif 

#endif 
