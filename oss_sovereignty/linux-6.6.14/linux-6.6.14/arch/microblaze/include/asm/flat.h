#ifndef _ASM_MICROBLAZE_FLAT_H
#define _ASM_MICROBLAZE_FLAT_H
#include <asm/unaligned.h>
static inline int flat_get_addr_from_rp(u32 __user *rp, u32 relval, u32 flags,
					u32 *addr)
{
	u32 *p = (__force u32 *)rp;
	if (relval & 0x80000000) {
		u32 val_hi, val_lo;
		val_hi = get_unaligned(p);
		val_lo = get_unaligned(p+1);
		*addr = ((val_hi & 0xffff) << 16) + (val_lo & 0xffff);
	} else {
		*addr = get_unaligned(p);
	}
	return 0;
}
static inline int
flat_put_addr_at_rp(u32 __user *rp, u32 addr, u32 relval)
{
	u32 *p = (__force u32 *)rp;
	if (relval & 0x80000000) {
		unsigned long val_hi = get_unaligned(p);
		unsigned long val_lo = get_unaligned(p + 1);
		val_hi = (val_hi & 0xffff0000) | addr >> 16;
		val_lo = (val_lo & 0xffff0000) | (addr & 0xffff);
		put_unaligned(val_hi, p);
		put_unaligned(val_lo, p+1);
	} else {
		put_unaligned(addr, p);
	}
	return 0;
}
#define	flat_get_relocate_addr(rel)	(rel & 0x7fffffff)
#endif  
