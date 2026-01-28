#ifndef _M68KNOMMU_CACHEFLUSH_H
#define _M68KNOMMU_CACHEFLUSH_H
#include <linux/mm.h>
#include <asm/mcfsim.h>
#define flush_cache_all()			__flush_cache_all()
#define flush_dcache_range(start, len)		__flush_dcache_all()
#define flush_icache_range(start, len)		__flush_icache_all()
void mcf_cache_push(void);
static inline void __clear_cache_all(void)
{
#ifdef CACHE_INVALIDATE
	__asm__ __volatile__ (
		"movec	%0, %%CACR\n\t"
		"nop\n\t"
		: : "r" (CACHE_INVALIDATE) );
#endif
}
static inline void __flush_cache_all(void)
{
#ifdef CACHE_PUSH
	mcf_cache_push();
#endif
	__clear_cache_all();
}
static inline void __flush_icache_all(void)
{
#ifdef CACHE_INVALIDATEI
	__asm__ __volatile__ (
		"movec	%0, %%CACR\n\t"
		"nop\n\t"
		: : "r" (CACHE_INVALIDATEI) );
#endif
}
static inline void __flush_dcache_all(void)
{
#ifdef CACHE_PUSH
	mcf_cache_push();
#endif
#ifdef CACHE_INVALIDATED
	__asm__ __volatile__ (
		"movec	%0, %%CACR\n\t"
		"nop\n\t"
		: : "r" (CACHE_INVALIDATED) );
#else
	__asm__ __volatile__ ( "nop" );
#endif
}
static inline void cache_push(unsigned long paddr, int len)
{
	__flush_cache_all();
}
static inline void cache_clear(unsigned long paddr, int len)
{
	__clear_cache_all();
}
#include <asm-generic/cacheflush.h>
#endif  
