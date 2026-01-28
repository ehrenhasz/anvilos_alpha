#ifndef _ASM_S390_ALTERNATIVE_H
#define _ASM_S390_ALTERNATIVE_H
#ifndef __ASSEMBLY__
#include <linux/types.h>
#include <linux/stddef.h>
#include <linux/stringify.h>
struct alt_instr {
	s32 instr_offset;	 
	s32 repl_offset;	 
	u16 facility;		 
	u8  instrlen;		 
} __packed;
void apply_alternative_instructions(void);
void apply_alternatives(struct alt_instr *start, struct alt_instr *end);
#define b_altinstr(num)		"664"#num
#define e_altinstr(num)		"665"#num
#define oldinstr_len		"662b-661b"
#define altinstr_len(num)	e_altinstr(num)"b-"b_altinstr(num)"b"
#define OLDINSTR(oldinstr) \
	"661:\n\t" oldinstr "\n662:\n"
#define ALTINSTR_ENTRY(facility, num)					\
	"\t.long 661b - .\n"			 	\
	"\t.long " b_altinstr(num)"b - .\n"	 	\
	"\t.word " __stringify(facility) "\n"	 	\
	"\t.byte " oldinstr_len "\n"		 	\
	"\t.org . - (" oldinstr_len ") + (" altinstr_len(num) ")\n"	\
	"\t.org . - (" altinstr_len(num) ") + (" oldinstr_len ")\n"
#define ALTINSTR_REPLACEMENT(altinstr, num)	 	\
	b_altinstr(num)":\n\t" altinstr "\n" e_altinstr(num) ":\n"
#define ALTERNATIVE(oldinstr, altinstr, facility) \
	".pushsection .altinstr_replacement, \"ax\"\n"			\
	ALTINSTR_REPLACEMENT(altinstr, 1)				\
	".popsection\n"							\
	OLDINSTR(oldinstr)						\
	".pushsection .altinstructions,\"a\"\n"				\
	ALTINSTR_ENTRY(facility, 1)					\
	".popsection\n"
#define ALTERNATIVE_2(oldinstr, altinstr1, facility1, altinstr2, facility2)\
	".pushsection .altinstr_replacement, \"ax\"\n"			\
	ALTINSTR_REPLACEMENT(altinstr1, 1)				\
	ALTINSTR_REPLACEMENT(altinstr2, 2)				\
	".popsection\n"							\
	OLDINSTR(oldinstr)						\
	".pushsection .altinstructions,\"a\"\n"				\
	ALTINSTR_ENTRY(facility1, 1)					\
	ALTINSTR_ENTRY(facility2, 2)					\
	".popsection\n"
#define alternative(oldinstr, altinstr, facility)			\
	asm_inline volatile(ALTERNATIVE(oldinstr, altinstr, facility) : : : "memory")
#define alternative_2(oldinstr, altinstr1, facility1, altinstr2, facility2) \
	asm_inline volatile(ALTERNATIVE_2(oldinstr, altinstr1, facility1,   \
				   altinstr2, facility2) ::: "memory")
#define alternative_input(oldinstr, newinstr, feature, input...)	\
	asm_inline volatile (ALTERNATIVE(oldinstr, newinstr, feature)	\
		: : input)
#define alternative_io(oldinstr, altinstr, facility, output, input...)	\
	asm_inline volatile(ALTERNATIVE(oldinstr, altinstr, facility)	\
		: output : input)
#define ASM_OUTPUT2(a...) a
#define ASM_NO_INPUT_CLOBBER(clobber...) : clobber
#endif  
#endif  
