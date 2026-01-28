#ifndef __ASM_ASM_ASID_H
#define __ASM_ASM_ASID_H
#include <linux/atomic.h>
#include <linux/compiler.h>
#include <linux/cpumask.h>
#include <linux/percpu.h>
#include <linux/spinlock.h>
struct asid_info
{
	atomic64_t	generation;
	unsigned long	*map;
	atomic64_t __percpu	*active;
	u64 __percpu		*reserved;
	u32			bits;
	raw_spinlock_t		lock;
	cpumask_t		flush_pending;
	unsigned int		ctxt_shift;
	void			(*flush_cpu_ctxt_cb)(void);
};
#define NUM_ASIDS(info)			(1UL << ((info)->bits))
#define NUM_CTXT_ASIDS(info)		(NUM_ASIDS(info) >> (info)->ctxt_shift)
#define active_asid(info, cpu)	*per_cpu_ptr((info)->active, cpu)
void asid_new_context(struct asid_info *info, atomic64_t *pasid,
		      unsigned int cpu, struct mm_struct *mm);
static inline void asid_check_context(struct asid_info *info,
				      atomic64_t *pasid, unsigned int cpu,
				      struct mm_struct *mm)
{
	u64 asid, old_active_asid;
	asid = atomic64_read(pasid);
	old_active_asid = atomic64_read(&active_asid(info, cpu));
	if (old_active_asid &&
	    !((asid ^ atomic64_read(&info->generation)) >> info->bits) &&
	    atomic64_cmpxchg_relaxed(&active_asid(info, cpu),
				     old_active_asid, asid))
		return;
	asid_new_context(info, pasid, cpu, mm);
}
int asid_allocator_init(struct asid_info *info,
			u32 bits, unsigned int asid_per_ctxt,
			void (*flush_cpu_ctxt_cb)(void));
#endif
