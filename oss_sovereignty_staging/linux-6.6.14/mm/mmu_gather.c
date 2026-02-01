#include <linux/gfp.h>
#include <linux/highmem.h>
#include <linux/kernel.h>
#include <linux/mmdebug.h>
#include <linux/mm_types.h>
#include <linux/mm_inline.h>
#include <linux/pagemap.h>
#include <linux/rcupdate.h>
#include <linux/smp.h>
#include <linux/swap.h>
#include <linux/rmap.h>

#include <asm/pgalloc.h>
#include <asm/tlb.h>

#ifndef CONFIG_MMU_GATHER_NO_GATHER

static bool tlb_next_batch(struct mmu_gather *tlb)
{
	struct mmu_gather_batch *batch;

	 
	if (tlb->delayed_rmap && tlb->active != &tlb->local)
		return false;

	batch = tlb->active;
	if (batch->next) {
		tlb->active = batch->next;
		return true;
	}

	if (tlb->batch_count == MAX_GATHER_BATCH_COUNT)
		return false;

	batch = (void *)__get_free_page(GFP_NOWAIT | __GFP_NOWARN);
	if (!batch)
		return false;

	tlb->batch_count++;
	batch->next = NULL;
	batch->nr   = 0;
	batch->max  = MAX_GATHER_BATCH;

	tlb->active->next = batch;
	tlb->active = batch;

	return true;
}

#ifdef CONFIG_SMP
static void tlb_flush_rmap_batch(struct mmu_gather_batch *batch, struct vm_area_struct *vma)
{
	for (int i = 0; i < batch->nr; i++) {
		struct encoded_page *enc = batch->encoded_pages[i];

		if (encoded_page_flags(enc)) {
			struct page *page = encoded_page_ptr(enc);
			page_remove_rmap(page, vma, false);
		}
	}
}

 
void tlb_flush_rmaps(struct mmu_gather *tlb, struct vm_area_struct *vma)
{
	if (!tlb->delayed_rmap)
		return;

	tlb_flush_rmap_batch(&tlb->local, vma);
	if (tlb->active != &tlb->local)
		tlb_flush_rmap_batch(tlb->active, vma);
	tlb->delayed_rmap = 0;
}
#endif

static void tlb_batch_pages_flush(struct mmu_gather *tlb)
{
	struct mmu_gather_batch *batch;

	for (batch = &tlb->local; batch && batch->nr; batch = batch->next) {
		struct encoded_page **pages = batch->encoded_pages;

		do {
			 
			unsigned int nr = min(512U, batch->nr);

			free_pages_and_swap_cache(pages, nr);
			pages += nr;
			batch->nr -= nr;

			cond_resched();
		} while (batch->nr);
	}
	tlb->active = &tlb->local;
}

static void tlb_batch_list_free(struct mmu_gather *tlb)
{
	struct mmu_gather_batch *batch, *next;

	for (batch = tlb->local.next; batch; batch = next) {
		next = batch->next;
		free_pages((unsigned long)batch, 0);
	}
	tlb->local.next = NULL;
}

bool __tlb_remove_page_size(struct mmu_gather *tlb, struct encoded_page *page, int page_size)
{
	struct mmu_gather_batch *batch;

	VM_BUG_ON(!tlb->end);

#ifdef CONFIG_MMU_GATHER_PAGE_SIZE
	VM_WARN_ON(tlb->page_size != page_size);
#endif

	batch = tlb->active;
	 
	batch->encoded_pages[batch->nr++] = page;
	if (batch->nr == batch->max) {
		if (!tlb_next_batch(tlb))
			return true;
		batch = tlb->active;
	}
	VM_BUG_ON_PAGE(batch->nr > batch->max, encoded_page_ptr(page));

	return false;
}

#endif  

#ifdef CONFIG_MMU_GATHER_TABLE_FREE

static void __tlb_remove_table_free(struct mmu_table_batch *batch)
{
	int i;

	for (i = 0; i < batch->nr; i++)
		__tlb_remove_table(batch->tables[i]);

	free_page((unsigned long)batch);
}

#ifdef CONFIG_MMU_GATHER_RCU_TABLE_FREE

 

static void tlb_remove_table_smp_sync(void *arg)
{
	 
}

void tlb_remove_table_sync_one(void)
{
	 
	smp_call_function(tlb_remove_table_smp_sync, NULL, 1);
}

static void tlb_remove_table_rcu(struct rcu_head *head)
{
	__tlb_remove_table_free(container_of(head, struct mmu_table_batch, rcu));
}

static void tlb_remove_table_free(struct mmu_table_batch *batch)
{
	call_rcu(&batch->rcu, tlb_remove_table_rcu);
}

#else  

static void tlb_remove_table_free(struct mmu_table_batch *batch)
{
	__tlb_remove_table_free(batch);
}

#endif  

 
static inline void tlb_table_invalidate(struct mmu_gather *tlb)
{
	if (tlb_needs_table_invalidate()) {
		 
		tlb_flush_mmu_tlbonly(tlb);
	}
}

static void tlb_remove_table_one(void *table)
{
	tlb_remove_table_sync_one();
	__tlb_remove_table(table);
}

static void tlb_table_flush(struct mmu_gather *tlb)
{
	struct mmu_table_batch **batch = &tlb->batch;

	if (*batch) {
		tlb_table_invalidate(tlb);
		tlb_remove_table_free(*batch);
		*batch = NULL;
	}
}

void tlb_remove_table(struct mmu_gather *tlb, void *table)
{
	struct mmu_table_batch **batch = &tlb->batch;

	if (*batch == NULL) {
		*batch = (struct mmu_table_batch *)__get_free_page(GFP_NOWAIT | __GFP_NOWARN);
		if (*batch == NULL) {
			tlb_table_invalidate(tlb);
			tlb_remove_table_one(table);
			return;
		}
		(*batch)->nr = 0;
	}

	(*batch)->tables[(*batch)->nr++] = table;
	if ((*batch)->nr == MAX_TABLE_BATCH)
		tlb_table_flush(tlb);
}

static inline void tlb_table_init(struct mmu_gather *tlb)
{
	tlb->batch = NULL;
}

#else  

static inline void tlb_table_flush(struct mmu_gather *tlb) { }
static inline void tlb_table_init(struct mmu_gather *tlb) { }

#endif  

static void tlb_flush_mmu_free(struct mmu_gather *tlb)
{
	tlb_table_flush(tlb);
#ifndef CONFIG_MMU_GATHER_NO_GATHER
	tlb_batch_pages_flush(tlb);
#endif
}

void tlb_flush_mmu(struct mmu_gather *tlb)
{
	tlb_flush_mmu_tlbonly(tlb);
	tlb_flush_mmu_free(tlb);
}

static void __tlb_gather_mmu(struct mmu_gather *tlb, struct mm_struct *mm,
			     bool fullmm)
{
	tlb->mm = mm;
	tlb->fullmm = fullmm;

#ifndef CONFIG_MMU_GATHER_NO_GATHER
	tlb->need_flush_all = 0;
	tlb->local.next = NULL;
	tlb->local.nr   = 0;
	tlb->local.max  = ARRAY_SIZE(tlb->__pages);
	tlb->active     = &tlb->local;
	tlb->batch_count = 0;
#endif
	tlb->delayed_rmap = 0;

	tlb_table_init(tlb);
#ifdef CONFIG_MMU_GATHER_PAGE_SIZE
	tlb->page_size = 0;
#endif

	__tlb_reset_range(tlb);
	inc_tlb_flush_pending(tlb->mm);
}

 
void tlb_gather_mmu(struct mmu_gather *tlb, struct mm_struct *mm)
{
	__tlb_gather_mmu(tlb, mm, false);
}

 
void tlb_gather_mmu_fullmm(struct mmu_gather *tlb, struct mm_struct *mm)
{
	__tlb_gather_mmu(tlb, mm, true);
}

 
void tlb_finish_mmu(struct mmu_gather *tlb)
{
	 
	if (mm_tlb_flush_nested(tlb->mm)) {
		 
		tlb->fullmm = 1;
		__tlb_reset_range(tlb);
		tlb->freed_tables = 1;
	}

	tlb_flush_mmu(tlb);

#ifndef CONFIG_MMU_GATHER_NO_GATHER
	tlb_batch_list_free(tlb);
#endif
	dec_tlb_flush_pending(tlb->mm);
}
