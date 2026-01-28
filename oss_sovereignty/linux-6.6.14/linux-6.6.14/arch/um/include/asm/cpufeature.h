#ifndef _ASM_UM_CPUFEATURE_H
#define _ASM_UM_CPUFEATURE_H
#include <asm/processor.h>
#if defined(__KERNEL__) && !defined(__ASSEMBLY__)
#include <asm/asm.h>
#include <linux/bitops.h>
extern const char * const x86_cap_flags[NCAPINTS*32];
extern const char * const x86_power_flags[32];
#define X86_CAP_FMT "%s"
#define x86_cap_flag(flag) x86_cap_flags[flag]
extern const char * const x86_bug_flags[NBUGINTS*32];
#define test_cpu_cap(c, bit)						\
	 test_bit(bit, (unsigned long *)((c)->x86_capability))
#define CHECK_BIT_IN_MASK_WORD(maskname, word, bit)	\
	(((bit)>>5)==(word) && (1UL<<((bit)&31) & maskname##word ))
#define cpu_has(c, bit)							\
	 test_cpu_cap(c, bit)
#define this_cpu_has(bit)						\
	(__builtin_constant_p(bit) && REQUIRED_MASK_BIT_SET(bit) ? 1 :	\
	 x86_this_cpu_test_bit(bit,					\
		(unsigned long __percpu *)&cpu_info.x86_capability))
#define cpu_feature_enabled(bit)	\
	(__builtin_constant_p(bit) && DISABLED_MASK_BIT_SET(bit) ? 0 : static_cpu_has(bit))
#define boot_cpu_has(bit)	cpu_has(&boot_cpu_data, bit)
#define set_cpu_cap(c, bit)	set_bit(bit, (unsigned long *)((c)->x86_capability))
extern void setup_clear_cpu_cap(unsigned int bit);
#define setup_force_cpu_cap(bit) do { \
	set_cpu_cap(&boot_cpu_data, bit);	\
	set_bit(bit, (unsigned long *)cpu_caps_set);	\
} while (0)
#define setup_force_cpu_bug(bit) setup_force_cpu_cap(bit)
static __always_inline bool _static_cpu_has(u16 bit)
{
	asm_volatile_goto("1: jmp 6f\n"
		 "2:\n"
		 ".skip -(((5f-4f) - (2b-1b)) > 0) * "
			 "((5f-4f) - (2b-1b)),0x90\n"
		 "3:\n"
		 ".section .altinstructions,\"a\"\n"
		 " .long 1b - .\n"		 
		 " .long 4f - .\n"		 
		 " .word %P[always]\n"		 
		 " .byte 3b - 1b\n"		 
		 " .byte 5f - 4f\n"		 
		 " .byte 3b - 2b\n"		 
		 ".previous\n"
		 ".section .altinstr_replacement,\"ax\"\n"
		 "4: jmp %l[t_no]\n"
		 "5:\n"
		 ".previous\n"
		 ".section .altinstructions,\"a\"\n"
		 " .long 1b - .\n"		 
		 " .long 0\n"			 
		 " .word %P[feature]\n"		 
		 " .byte 3b - 1b\n"		 
		 " .byte 0\n"			 
		 " .byte 0\n"			 
		 ".previous\n"
		 ".section .altinstr_aux,\"ax\"\n"
		 "6:\n"
		 " testb %[bitnum],%[cap_byte]\n"
		 " jnz %l[t_yes]\n"
		 " jmp %l[t_no]\n"
		 ".previous\n"
		 : : [feature]  "i" (bit),
		     [always]   "i" (X86_FEATURE_ALWAYS),
		     [bitnum]   "i" (1 << (bit & 7)),
		     [cap_byte] "m" (((const char *)boot_cpu_data.x86_capability)[bit >> 3])
		 : : t_yes, t_no);
t_yes:
	return true;
t_no:
	return false;
}
#define static_cpu_has(bit)					\
(								\
	__builtin_constant_p(boot_cpu_has(bit)) ?		\
		boot_cpu_has(bit) :				\
		_static_cpu_has(bit)				\
)
#define cpu_has_bug(c, bit)		cpu_has(c, (bit))
#define set_cpu_bug(c, bit)		set_cpu_cap(c, (bit))
#define static_cpu_has_bug(bit)		static_cpu_has((bit))
#define boot_cpu_has_bug(bit)		cpu_has_bug(&boot_cpu_data, (bit))
#define boot_cpu_set_bug(bit)		set_cpu_cap(&boot_cpu_data, (bit))
#define MAX_CPU_FEATURES		(NCAPINTS * 32)
#define cpu_have_feature		boot_cpu_has
#define CPU_FEATURE_TYPEFMT		"x86,ven%04Xfam%04Xmod%04X"
#define CPU_FEATURE_TYPEVAL		boot_cpu_data.x86_vendor, boot_cpu_data.x86, \
					boot_cpu_data.x86_model
#endif  
#endif  
