#ifndef __ASM_MTE_KASAN_H
#define __ASM_MTE_KASAN_H
#include <asm/compiler.h>
#include <asm/cputype.h>
#include <asm/mte-def.h>
#ifndef __ASSEMBLY__
#include <linux/types.h>
#ifdef CONFIG_KASAN_HW_TAGS
DECLARE_STATIC_KEY_FALSE(mte_async_or_asymm_mode);
static inline bool system_uses_mte_async_or_asymm_mode(void)
{
	return static_branch_unlikely(&mte_async_or_asymm_mode);
}
#else  
static inline bool system_uses_mte_async_or_asymm_mode(void)
{
	return false;
}
#endif  
#ifdef CONFIG_ARM64_MTE
static inline void mte_disable_tco(void)
{
	asm volatile(ALTERNATIVE("nop", SET_PSTATE_TCO(0),
				 ARM64_MTE, CONFIG_KASAN_HW_TAGS));
}
static inline void mte_enable_tco(void)
{
	asm volatile(ALTERNATIVE("nop", SET_PSTATE_TCO(1),
				 ARM64_MTE, CONFIG_KASAN_HW_TAGS));
}
static inline void __mte_disable_tco_async(void)
{
	if (system_uses_mte_async_or_asymm_mode())
		mte_disable_tco();
}
static inline void __mte_enable_tco_async(void)
{
	if (system_uses_mte_async_or_asymm_mode())
		mte_enable_tco();
}
static inline u8 mte_get_ptr_tag(void *ptr)
{
	u8 tag = 0xF0 | (u8)(((u64)(ptr)) >> MTE_TAG_SHIFT);
	return tag;
}
static inline u8 mte_get_mem_tag(void *addr)
{
	asm(__MTE_PREAMBLE "ldg %0, [%0]"
		: "+r" (addr));
	return mte_get_ptr_tag(addr);
}
static inline u8 mte_get_random_tag(void)
{
	void *addr;
	asm(__MTE_PREAMBLE "irg %0, %0"
		: "=r" (addr));
	return mte_get_ptr_tag(addr);
}
static inline u64 __stg_post(u64 p)
{
	asm volatile(__MTE_PREAMBLE "stg %0, [%0], #16"
		     : "+r"(p)
		     :
		     : "memory");
	return p;
}
static inline u64 __stzg_post(u64 p)
{
	asm volatile(__MTE_PREAMBLE "stzg %0, [%0], #16"
		     : "+r"(p)
		     :
		     : "memory");
	return p;
}
static inline void __dc_gva(u64 p)
{
	asm volatile(__MTE_PREAMBLE "dc gva, %0" : : "r"(p) : "memory");
}
static inline void __dc_gzva(u64 p)
{
	asm volatile(__MTE_PREAMBLE "dc gzva, %0" : : "r"(p) : "memory");
}
static inline void mte_set_mem_tag_range(void *addr, size_t size, u8 tag,
					 bool init)
{
	u64 curr, mask, dczid, dczid_bs, dczid_dzp, end1, end2, end3;
	dczid = read_cpuid(DCZID_EL0);
	dczid_bs = 4ul << (dczid & 0xf);
	dczid_dzp = (dczid >> 4) & 1;
	curr = (u64)__tag_set(addr, tag);
	mask = dczid_bs - 1;
	end1 = curr | mask;
	end3 = curr + size;
	end2 = end3 & ~mask;
#define SET_MEMTAG_RANGE(stg_post, dc_gva)		\
	do {						\
		if (!dczid_dzp && size >= 2 * dczid_bs) {\
			do {				\
				curr = stg_post(curr);	\
			} while (curr < end1);		\
							\
			do {				\
				dc_gva(curr);		\
				curr += dczid_bs;	\
			} while (curr < end2);		\
		}					\
							\
		while (curr < end3)			\
			curr = stg_post(curr);		\
	} while (0)
	if (init)
		SET_MEMTAG_RANGE(__stzg_post, __dc_gzva);
	else
		SET_MEMTAG_RANGE(__stg_post, __dc_gva);
#undef SET_MEMTAG_RANGE
}
void mte_enable_kernel_sync(void);
void mte_enable_kernel_async(void);
void mte_enable_kernel_asymm(void);
#else  
static inline void mte_disable_tco(void)
{
}
static inline void mte_enable_tco(void)
{
}
static inline void __mte_disable_tco_async(void)
{
}
static inline void __mte_enable_tco_async(void)
{
}
static inline u8 mte_get_ptr_tag(void *ptr)
{
	return 0xFF;
}
static inline u8 mte_get_mem_tag(void *addr)
{
	return 0xFF;
}
static inline u8 mte_get_random_tag(void)
{
	return 0xFF;
}
static inline void mte_set_mem_tag_range(void *addr, size_t size,
						u8 tag, bool init)
{
}
static inline void mte_enable_kernel_sync(void)
{
}
static inline void mte_enable_kernel_async(void)
{
}
static inline void mte_enable_kernel_asymm(void)
{
}
#endif  
#endif  
#endif  
