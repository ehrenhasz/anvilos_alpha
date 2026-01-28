#ifndef _ASM_TIMEX_H
#define _ASM_TIMEX_H
#ifdef __KERNEL__
#include <linux/compiler.h>
#include <asm/cpu.h>
#include <asm/cpu-features.h>
#include <asm/mipsregs.h>
#include <asm/cpu-type.h>
#define CLOCK_TICK_RATE 1193182
typedef unsigned int cycles_t;
static inline int can_use_mips_counter(unsigned int prid)
{
	int comp = (prid & PRID_COMP_MASK) != PRID_COMP_LEGACY;
	if (__builtin_constant_p(cpu_has_counter) && !cpu_has_counter)
		return 0;
	else if (__builtin_constant_p(cpu_has_mips_r) && cpu_has_mips_r)
		return 1;
	else if (likely(!__builtin_constant_p(cpu_has_mips_r) && comp))
		return 1;
	if (!__builtin_constant_p(cpu_has_counter))
		asm volatile("" : "=m" (cpu_data[0].options));
	if (likely(cpu_has_counter &&
		   prid > (PRID_IMP_R4000 | PRID_REV_ENCODE_44(15, 15))))
		return 1;
	else
		return 0;
}
static inline cycles_t get_cycles(void)
{
	if (can_use_mips_counter(read_c0_prid()))
		return read_c0_count();
	else
		return 0;	 
}
#define get_cycles get_cycles
static inline unsigned long random_get_entropy(void)
{
	unsigned int c0_random;
	if (can_use_mips_counter(read_c0_prid()))
		return read_c0_count();
	if (cpu_has_3kex)
		c0_random = (read_c0_random() >> 8) & 0x3f;
	else
		c0_random = read_c0_random() & 0x3f;
	return (random_get_entropy_fallback() << 6) | (0x3f - c0_random);
}
#define random_get_entropy random_get_entropy
#endif  
#endif  
