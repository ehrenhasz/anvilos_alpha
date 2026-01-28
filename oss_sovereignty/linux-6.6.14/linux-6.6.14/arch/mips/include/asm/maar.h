#ifndef __MIPS_ASM_MIPS_MAAR_H__
#define __MIPS_ASM_MIPS_MAAR_H__
#include <asm/hazards.h>
#include <asm/mipsregs.h>
unsigned platform_maar_init(unsigned num_pairs);
static inline void write_maar_pair(unsigned idx, phys_addr_t lower,
				   phys_addr_t upper, unsigned attrs)
{
	BUG_ON(lower & (0xffff | ~(MIPS_MAAR_ADDR << 4)));
	BUG_ON(((upper & 0xffff) != 0xffff)
		|| ((upper & ~0xffffull) & ~(MIPS_MAAR_ADDR << 4)));
	attrs |= MIPS_MAAR_VL;
	write_c0_maari(idx << 1);
	back_to_back_c0_hazard();
	write_c0_maar(((upper >> 4) & MIPS_MAAR_ADDR) | attrs);
	back_to_back_c0_hazard();
#ifdef CONFIG_XPA
	upper >>= MIPS_MAARX_ADDR_SHIFT;
	writex_c0_maar(((upper >> 4) & MIPS_MAARX_ADDR) | MIPS_MAARX_VH);
	back_to_back_c0_hazard();
#endif
	write_c0_maari((idx << 1) | 0x1);
	back_to_back_c0_hazard();
	write_c0_maar((lower >> 4) | attrs);
	back_to_back_c0_hazard();
#ifdef CONFIG_XPA
	lower >>= MIPS_MAARX_ADDR_SHIFT;
	writex_c0_maar(((lower >> 4) & MIPS_MAARX_ADDR) | MIPS_MAARX_VH);
	back_to_back_c0_hazard();
#endif
}
extern void maar_init(void);
struct maar_config {
	phys_addr_t lower;
	phys_addr_t upper;
	unsigned attrs;
};
static inline unsigned maar_config(const struct maar_config *cfg,
				   unsigned num_cfg, unsigned num_pairs)
{
	unsigned i;
	for (i = 0; i < min(num_cfg, num_pairs); i++)
		write_maar_pair(i, cfg[i].lower, cfg[i].upper, cfg[i].attrs);
	return i;
}
#endif  
