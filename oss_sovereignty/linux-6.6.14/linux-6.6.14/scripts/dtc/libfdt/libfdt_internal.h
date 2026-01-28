#ifndef LIBFDT_INTERNAL_H
#define LIBFDT_INTERNAL_H
#include <fdt.h>
#define FDT_ALIGN(x, a)		(((x) + (a) - 1) & ~((a) - 1))
#define FDT_TAGALIGN(x)		(FDT_ALIGN((x), FDT_TAGSIZE))
int32_t fdt_ro_probe_(const void *fdt);
#define FDT_RO_PROBE(fdt)					\
	{							\
		int32_t totalsize_;				\
		if ((totalsize_ = fdt_ro_probe_(fdt)) < 0)	\
			return totalsize_;			\
	}
int fdt_check_node_offset_(const void *fdt, int offset);
int fdt_check_prop_offset_(const void *fdt, int offset);
const char *fdt_find_string_(const char *strtab, int tabsize, const char *s);
int fdt_node_end_offset_(void *fdt, int nodeoffset);
static inline const void *fdt_offset_ptr_(const void *fdt, int offset)
{
	return (const char *)fdt + fdt_off_dt_struct(fdt) + offset;
}
static inline void *fdt_offset_ptr_w_(void *fdt, int offset)
{
	return (void *)(uintptr_t)fdt_offset_ptr_(fdt, offset);
}
static inline const struct fdt_reserve_entry *fdt_mem_rsv_(const void *fdt, int n)
{
	const struct fdt_reserve_entry *rsv_table =
		(const struct fdt_reserve_entry *)
		((const char *)fdt + fdt_off_mem_rsvmap(fdt));
	return rsv_table + n;
}
static inline struct fdt_reserve_entry *fdt_mem_rsv_w_(void *fdt, int n)
{
	return (void *)(uintptr_t)fdt_mem_rsv_(fdt, n);
}
static inline uint32_t fdt32_ld_(const fdt32_t *p)
{
	return fdt32_to_cpu(*p);
}
static inline uint64_t fdt64_ld_(const fdt64_t *p)
{
	return fdt64_to_cpu(*p);
}
#define FDT_SW_MAGIC		(~FDT_MAGIC)
#ifndef FDT_ASSUME_MASK
#define FDT_ASSUME_MASK 0
#endif
enum {
	ASSUME_PERFECT		= 0xff,
	ASSUME_VALID_DTB	= 1 << 0,
	ASSUME_VALID_INPUT	= 1 << 1,
	ASSUME_LATEST		= 1 << 2,
	ASSUME_NO_ROLLBACK	= 1 << 3,
	ASSUME_LIBFDT_ORDER	= 1 << 4,
	ASSUME_LIBFDT_FLAWLESS	= 1 << 5,
};
static inline bool can_assume_(int mask)
{
	return FDT_ASSUME_MASK & mask;
}
#define can_assume(_assume)	can_assume_(ASSUME_ ## _assume)
#endif  
