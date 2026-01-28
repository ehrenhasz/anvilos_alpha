#ifndef _ASM_ALTERNATIVE_H
#define _ASM_ALTERNATIVE_H
#ifndef __ASSEMBLY__
#include <linux/types.h>
#include <linux/stddef.h>
#include <linux/stringify.h>
#include <asm/asm.h>
struct alt_instr {
	s32 instr_offset;	 
	s32 replace_offset;	 
	u16 feature;		 
	u8  instrlen;		 
	u8  replacementlen;	 
} __packed;
extern int alternatives_patched;
extern struct alt_instr __alt_instructions[], __alt_instructions_end[];
extern void alternative_instructions(void);
extern void apply_alternatives(struct alt_instr *start, struct alt_instr *end);
#define b_replacement(num)	"664"#num
#define e_replacement(num)	"665"#num
#define alt_end_marker		"663"
#define alt_slen		"662b-661b"
#define alt_total_slen		alt_end_marker"b-661b"
#define alt_rlen(num)		e_replacement(num)"f-"b_replacement(num)"f"
#define __OLDINSTR(oldinstr, num)					\
	"661:\n\t" oldinstr "\n662:\n"					\
	".fill -(((" alt_rlen(num) ")-(" alt_slen ")) > 0) * "		\
		"((" alt_rlen(num) ")-(" alt_slen ")) / 4, 4, 0x03400000\n"
#define OLDINSTR(oldinstr, num)						\
	__OLDINSTR(oldinstr, num)					\
	alt_end_marker ":\n"
#define alt_max_short(a, b)	"((" a ") ^ (((" a ") ^ (" b ")) & -(-((" a ") < (" b ")))))"
#define OLDINSTR_2(oldinstr, num1, num2) \
	"661:\n\t" oldinstr "\n662:\n"								\
	".fill -((" alt_max_short(alt_rlen(num1), alt_rlen(num2)) " - (" alt_slen ")) > 0) * "	\
		"(" alt_max_short(alt_rlen(num1), alt_rlen(num2)) " - (" alt_slen ")) / 4, "	\
		"4, 0x03400000\n"	\
	alt_end_marker ":\n"
#define ALTINSTR_ENTRY(feature, num)					      \
	" .long 661b - .\n"				  \
	" .long " b_replacement(num)"f - .\n"		  \
	" .short " __stringify(feature) "\n"		  \
	" .byte " alt_total_slen "\n"			  \
	" .byte " alt_rlen(num) "\n"			 
#define ALTINSTR_REPLACEMENT(newinstr, feature, num)	      \
	b_replacement(num)":\n\t" newinstr "\n" e_replacement(num) ":\n\t"
#define ALTERNATIVE(oldinstr, newinstr, feature)			\
	OLDINSTR(oldinstr, 1)						\
	".pushsection .altinstructions,\"a\"\n"				\
	ALTINSTR_ENTRY(feature, 1)					\
	".popsection\n"							\
	".subsection 1\n" \
	ALTINSTR_REPLACEMENT(newinstr, feature, 1)			\
	".previous\n"
#define ALTERNATIVE_2(oldinstr, newinstr1, feature1, newinstr2, feature2)\
	OLDINSTR_2(oldinstr, 1, 2)					\
	".pushsection .altinstructions,\"a\"\n"				\
	ALTINSTR_ENTRY(feature1, 1)					\
	ALTINSTR_ENTRY(feature2, 2)					\
	".popsection\n"							\
	".subsection 1\n" \
	ALTINSTR_REPLACEMENT(newinstr1, feature1, 1)			\
	ALTINSTR_REPLACEMENT(newinstr2, feature2, 2)			\
	".previous\n"
#define alternative(oldinstr, newinstr, feature)			\
	(asm volatile (ALTERNATIVE(oldinstr, newinstr, feature) : : : "memory"))
#define alternative_2(oldinstr, newinstr1, feature1, newinstr2, feature2) \
	(asm volatile(ALTERNATIVE_2(oldinstr, newinstr1, feature1, newinstr2, feature2) ::: "memory"))
#endif  
#endif  
