
 

#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/hugetlb.h>
#include <linux/delay.h>
#include <linux/timex.h>
#include <linux/srcu.h>
#include <asm/processor.h>
#include "gru.h"
#include "grutables.h"
#include <asm/uv/uv_hub.h>

#define gru_random()	get_cycles()

 
static inline int get_off_blade_tgh(struct gru_state *gru)
{
	int n;

	n = GRU_NUM_TGH - gru->gs_tgh_first_remote;
	n = gru_random() % n;
	n += gru->gs_tgh_first_remote;
	return n;
}

static inline int get_on_blade_tgh(struct gru_state *gru)
{
	return uv_blade_processor_id() >> gru->gs_tgh_local_shift;
}

static struct gru_tlb_global_handle *get_lock_tgh_handle(struct gru_state
							 *gru)
{
	struct gru_tlb_global_handle *tgh;
	int n;

	preempt_disable();
	if (uv_numa_blade_id() == gru->gs_blade_id)
		n = get_on_blade_tgh(gru);
	else
		n = get_off_blade_tgh(gru);
	tgh = get_tgh_by_index(gru, n);
	lock_tgh_handle(tgh);

	return tgh;
}

static void get_unlock_tgh_handle(struct gru_tlb_global_handle *tgh)
{
	unlock_tgh_handle(tgh);
	preempt_enable();
}

 

void gru_flush_tlb_range(struct gru_mm_struct *gms, unsigned long start,
			 unsigned long len)
{
	struct gru_state *gru;
	struct gru_mm_tracker *asids;
	struct gru_tlb_global_handle *tgh;
	unsigned long num;
	int grupagesize, pagesize, pageshift, gid, asid;

	 
	pageshift = PAGE_SHIFT;
	pagesize = (1UL << pageshift);
	grupagesize = GRU_PAGESIZE(pageshift);
	num = min(((len + pagesize - 1) >> pageshift), GRUMAXINVAL);

	STAT(flush_tlb);
	gru_dbg(grudev, "gms %p, start 0x%lx, len 0x%lx, asidmap 0x%lx\n", gms,
		start, len, gms->ms_asidmap[0]);

	spin_lock(&gms->ms_asid_lock);
	for_each_gru_in_bitmap(gid, gms->ms_asidmap) {
		STAT(flush_tlb_gru);
		gru = GID_TO_GRU(gid);
		asids = gms->ms_asids + gid;
		asid = asids->mt_asid;
		if (asids->mt_ctxbitmap && asid) {
			STAT(flush_tlb_gru_tgh);
			asid = GRUASID(asid, start);
			gru_dbg(grudev,
	"  FLUSH gruid %d, asid 0x%x, vaddr 0x%lx, vamask 0x%x, num %ld, cbmap 0x%x\n",
			      gid, asid, start, grupagesize, num, asids->mt_ctxbitmap);
			tgh = get_lock_tgh_handle(gru);
			tgh_invalidate(tgh, start, ~0, asid, grupagesize, 0,
				       num - 1, asids->mt_ctxbitmap);
			get_unlock_tgh_handle(tgh);
		} else {
			STAT(flush_tlb_gru_zero_asid);
			asids->mt_asid = 0;
			__clear_bit(gru->gs_gid, gms->ms_asidmap);
			gru_dbg(grudev,
	"  CLEARASID gruid %d, asid 0x%x, cbtmap 0x%x, asidmap 0x%lx\n",
				gid, asid, asids->mt_ctxbitmap,
				gms->ms_asidmap[0]);
		}
	}
	spin_unlock(&gms->ms_asid_lock);
}

 
void gru_flush_all_tlb(struct gru_state *gru)
{
	struct gru_tlb_global_handle *tgh;

	gru_dbg(grudev, "gid %d\n", gru->gs_gid);
	tgh = get_lock_tgh_handle(gru);
	tgh_invalidate(tgh, 0, ~0, 0, 1, 1, GRUMAXINVAL - 1, 0xffff);
	get_unlock_tgh_handle(tgh);
}

 
static int gru_invalidate_range_start(struct mmu_notifier *mn,
			const struct mmu_notifier_range *range)
{
	struct gru_mm_struct *gms = container_of(mn, struct gru_mm_struct,
						 ms_notifier);

	STAT(mmu_invalidate_range);
	atomic_inc(&gms->ms_range_active);
	gru_dbg(grudev, "gms %p, start 0x%lx, end 0x%lx, act %d\n", gms,
		range->start, range->end, atomic_read(&gms->ms_range_active));
	gru_flush_tlb_range(gms, range->start, range->end - range->start);

	return 0;
}

static void gru_invalidate_range_end(struct mmu_notifier *mn,
			const struct mmu_notifier_range *range)
{
	struct gru_mm_struct *gms = container_of(mn, struct gru_mm_struct,
						 ms_notifier);

	 
	(void)atomic_dec_and_test(&gms->ms_range_active);

	wake_up_all(&gms->ms_wait_queue);
	gru_dbg(grudev, "gms %p, start 0x%lx, end 0x%lx\n",
		gms, range->start, range->end);
}

static struct mmu_notifier *gru_alloc_notifier(struct mm_struct *mm)
{
	struct gru_mm_struct *gms;

	gms = kzalloc(sizeof(*gms), GFP_KERNEL);
	if (!gms)
		return ERR_PTR(-ENOMEM);
	STAT(gms_alloc);
	spin_lock_init(&gms->ms_asid_lock);
	init_waitqueue_head(&gms->ms_wait_queue);

	return &gms->ms_notifier;
}

static void gru_free_notifier(struct mmu_notifier *mn)
{
	kfree(container_of(mn, struct gru_mm_struct, ms_notifier));
	STAT(gms_free);
}

static const struct mmu_notifier_ops gru_mmuops = {
	.invalidate_range_start	= gru_invalidate_range_start,
	.invalidate_range_end	= gru_invalidate_range_end,
	.alloc_notifier		= gru_alloc_notifier,
	.free_notifier		= gru_free_notifier,
};

struct gru_mm_struct *gru_register_mmu_notifier(void)
{
	struct mmu_notifier *mn;

	mn = mmu_notifier_get_locked(&gru_mmuops, current->mm);
	if (IS_ERR(mn))
		return ERR_CAST(mn);

	return container_of(mn, struct gru_mm_struct, ms_notifier);
}

void gru_drop_mmu_notifier(struct gru_mm_struct *gms)
{
	mmu_notifier_put(&gms->ms_notifier);
}

 
#define MAX_LOCAL_TGH	16

void gru_tgh_flush_init(struct gru_state *gru)
{
	int cpus, shift = 0, n;

	cpus = uv_blade_nr_possible_cpus(gru->gs_blade_id);

	 
	if (cpus) {
		n = 1 << fls(cpus - 1);

		 
		shift = max(0, fls(n - 1) - fls(MAX_LOCAL_TGH - 1));
	}
	gru->gs_tgh_local_shift = shift;

	 
	gru->gs_tgh_first_remote = (cpus + (1 << shift) - 1) >> shift;

}
