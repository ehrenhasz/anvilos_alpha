#ifndef _ASM_IA64_SN_SN_SAL_H
#define _ASM_IA64_SN_SN_SAL_H
#include <linux/types.h>
#include <asm/sal.h>
#define  SN_SAL_GET_PARTITION_ADDR		   0x02000009
#define  SN_SAL_MEMPROTECT                         0x0200003e
#define  SN_SAL_WATCHLIST_ALLOC			   0x02000070
#define  SN_SAL_WATCHLIST_FREE			   0x02000071
#define SALRET_MORE_PASSES	1
#define SALRET_OK		0
#define SALRET_NOT_IMPLEMENTED	(-1)
#define SALRET_INVALID_ARG	(-2)
#define SALRET_ERROR		(-3)
static inline s64
sn_partition_reserved_page_pa(u64 buf, u64 *cookie, u64 *addr, u64 *len)
{
	struct ia64_sal_retval rv;
	ia64_sal_oemcall_reentrant(&rv, SN_SAL_GET_PARTITION_ADDR, *cookie,
				   *addr, buf, *len, 0, 0, 0);
	*cookie = rv.v0;
	*addr = rv.v1;
	*len = rv.v2;
	return rv.status;
}
static inline int
sn_change_memprotect(u64 paddr, u64 len, u64 perms, u64 *nasid_array)
{
	struct ia64_sal_retval ret_stuff;
	ia64_sal_oemcall_nolock(&ret_stuff, SN_SAL_MEMPROTECT, paddr, len,
				(u64)nasid_array, perms, 0, 0, 0);
	return ret_stuff.status;
}
#define SN_MEMPROT_ACCESS_CLASS_0		0x14a080
#define SN_MEMPROT_ACCESS_CLASS_1		0x2520c2
#define SN_MEMPROT_ACCESS_CLASS_2		0x14a1ca
#define SN_MEMPROT_ACCESS_CLASS_3		0x14a290
#define SN_MEMPROT_ACCESS_CLASS_6		0x084080
#define SN_MEMPROT_ACCESS_CLASS_7		0x021080
union sn_watchlist_u {
	u64     val;
	struct {
		u64	blade	: 16,
			size	: 32,
			filler	: 16;
	};
};
static inline int
sn_mq_watchlist_alloc(int blade, void *mq, unsigned int mq_size,
				unsigned long *intr_mmr_offset)
{
	struct ia64_sal_retval rv;
	unsigned long addr;
	union sn_watchlist_u size_blade;
	int watchlist;
	addr = (unsigned long)mq;
	size_blade.size = mq_size;
	size_blade.blade = blade;
	ia64_sal_oemcall_nolock(&rv, SN_SAL_WATCHLIST_ALLOC, addr,
			size_blade.val, (u64)intr_mmr_offset,
			(u64)&watchlist, 0, 0, 0);
	if (rv.status < 0)
		return rv.status;
	return watchlist;
}
static inline int
sn_mq_watchlist_free(int blade, int watchlist_num)
{
	struct ia64_sal_retval rv;
	ia64_sal_oemcall_nolock(&rv, SN_SAL_WATCHLIST_FREE, blade,
			watchlist_num, 0, 0, 0, 0, 0);
	return rv.status;
}
#endif  
