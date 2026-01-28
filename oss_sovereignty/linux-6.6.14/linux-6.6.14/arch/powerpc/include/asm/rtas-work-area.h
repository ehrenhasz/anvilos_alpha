#ifndef _ASM_POWERPC_RTAS_WORK_AREA_H
#define _ASM_POWERPC_RTAS_WORK_AREA_H
#include <linux/build_bug.h>
#include <linux/sizes.h>
#include <linux/types.h>
#include <asm/page.h>
struct rtas_work_area {
	char *buf;
	size_t size;
};
enum {
	RTAS_WORK_AREA_MAX_ALLOC_SZ = SZ_128K,
};
#define rtas_work_area_alloc(size_) ({				\
	static_assert(__builtin_constant_p(size_));		\
	static_assert((size_) > 0);				\
	static_assert((size_) <= RTAS_WORK_AREA_MAX_ALLOC_SZ);	\
	__rtas_work_area_alloc(size_);				\
})
struct rtas_work_area *__rtas_work_area_alloc(size_t size);
void rtas_work_area_free(struct rtas_work_area *area);
static inline char *rtas_work_area_raw_buf(const struct rtas_work_area *area)
{
	return area->buf;
}
static inline size_t rtas_work_area_size(const struct rtas_work_area *area)
{
	return area->size;
}
static inline phys_addr_t rtas_work_area_phys(const struct rtas_work_area *area)
{
	return __pa(area->buf);
}
#ifdef CONFIG_PPC_PSERIES
void rtas_work_area_reserve_arena(phys_addr_t limit);
#else  
static inline void rtas_work_area_reserve_arena(phys_addr_t limit) {}
#endif  
#endif  
