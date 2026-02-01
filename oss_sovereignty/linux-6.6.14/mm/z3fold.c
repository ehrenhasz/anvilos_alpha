
 

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/atomic.h>
#include <linux/sched.h>
#include <linux/cpumask.h>
#include <linux/list.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/page-flags.h>
#include <linux/migrate.h>
#include <linux/node.h>
#include <linux/compaction.h>
#include <linux/percpu.h>
#include <linux/preempt.h>
#include <linux/workqueue.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/zpool.h>
#include <linux/kmemleak.h>

 
#define NCHUNKS_ORDER	6

#define CHUNK_SHIFT	(PAGE_SHIFT - NCHUNKS_ORDER)
#define CHUNK_SIZE	(1 << CHUNK_SHIFT)
#define ZHDR_SIZE_ALIGNED round_up(sizeof(struct z3fold_header), CHUNK_SIZE)
#define ZHDR_CHUNKS	(ZHDR_SIZE_ALIGNED >> CHUNK_SHIFT)
#define TOTAL_CHUNKS	(PAGE_SIZE >> CHUNK_SHIFT)
#define NCHUNKS		(TOTAL_CHUNKS - ZHDR_CHUNKS)

#define BUDDY_MASK	(0x3)
#define BUDDY_SHIFT	2
#define SLOTS_ALIGN	(0x40)

 
struct z3fold_pool;

enum buddy {
	HEADLESS = 0,
	FIRST,
	MIDDLE,
	LAST,
	BUDDIES_MAX = LAST
};

struct z3fold_buddy_slots {
	 
	unsigned long slot[BUDDY_MASK + 1];
	unsigned long pool;  
	rwlock_t lock;
};
#define HANDLE_FLAG_MASK	(0x03)

 
struct z3fold_header {
	struct list_head buddy;
	spinlock_t page_lock;
	struct kref refcount;
	struct work_struct work;
	struct z3fold_buddy_slots *slots;
	struct z3fold_pool *pool;
	short cpu;
	unsigned short first_chunks;
	unsigned short middle_chunks;
	unsigned short last_chunks;
	unsigned short start_middle;
	unsigned short first_num:2;
	unsigned short mapped_count:2;
	unsigned short foreign_handles:2;
};

 
struct z3fold_pool {
	const char *name;
	spinlock_t lock;
	spinlock_t stale_lock;
	struct list_head *unbuddied;
	struct list_head stale;
	atomic64_t pages_nr;
	struct kmem_cache *c_handle;
	struct workqueue_struct *compact_wq;
	struct workqueue_struct *release_wq;
	struct work_struct work;
};

 
enum z3fold_page_flags {
	PAGE_HEADLESS = 0,
	MIDDLE_CHUNK_MAPPED,
	NEEDS_COMPACTING,
	PAGE_STALE,
	PAGE_CLAIMED,  
	PAGE_MIGRATED,  
};

 
enum z3fold_handle_flags {
	HANDLES_NOFREE = 0,
};

 
static struct z3fold_header *__z3fold_alloc(struct z3fold_pool *, size_t, bool);
static void compact_page_work(struct work_struct *w);

 

 
static int size_to_chunks(size_t size)
{
	return (size + CHUNK_SIZE - 1) >> CHUNK_SHIFT;
}

#define for_each_unbuddied_list(_iter, _begin) \
	for ((_iter) = (_begin); (_iter) < NCHUNKS; (_iter)++)

static inline struct z3fold_buddy_slots *alloc_slots(struct z3fold_pool *pool,
							gfp_t gfp)
{
	struct z3fold_buddy_slots *slots = kmem_cache_zalloc(pool->c_handle,
							     gfp);

	if (slots) {
		 
		kmemleak_not_leak(slots);
		slots->pool = (unsigned long)pool;
		rwlock_init(&slots->lock);
	}

	return slots;
}

static inline struct z3fold_pool *slots_to_pool(struct z3fold_buddy_slots *s)
{
	return (struct z3fold_pool *)(s->pool & ~HANDLE_FLAG_MASK);
}

static inline struct z3fold_buddy_slots *handle_to_slots(unsigned long handle)
{
	return (struct z3fold_buddy_slots *)(handle & ~(SLOTS_ALIGN - 1));
}

 
static inline void z3fold_page_lock(struct z3fold_header *zhdr)
{
	spin_lock(&zhdr->page_lock);
}

 
static inline int z3fold_page_trylock(struct z3fold_header *zhdr)
{
	return spin_trylock(&zhdr->page_lock);
}

 
static inline void z3fold_page_unlock(struct z3fold_header *zhdr)
{
	spin_unlock(&zhdr->page_lock);
}

 
static inline struct z3fold_header *get_z3fold_header(unsigned long handle)
{
	struct z3fold_buddy_slots *slots;
	struct z3fold_header *zhdr;
	int locked = 0;

	if (!(handle & (1 << PAGE_HEADLESS))) {
		slots = handle_to_slots(handle);
		do {
			unsigned long addr;

			read_lock(&slots->lock);
			addr = *(unsigned long *)handle;
			zhdr = (struct z3fold_header *)(addr & PAGE_MASK);
			locked = z3fold_page_trylock(zhdr);
			read_unlock(&slots->lock);
			if (locked) {
				struct page *page = virt_to_page(zhdr);

				if (!test_bit(PAGE_MIGRATED, &page->private))
					break;
				z3fold_page_unlock(zhdr);
			}
			cpu_relax();
		} while (true);
	} else {
		zhdr = (struct z3fold_header *)(handle & PAGE_MASK);
	}

	return zhdr;
}

static inline void put_z3fold_header(struct z3fold_header *zhdr)
{
	struct page *page = virt_to_page(zhdr);

	if (!test_bit(PAGE_HEADLESS, &page->private))
		z3fold_page_unlock(zhdr);
}

static inline void free_handle(unsigned long handle, struct z3fold_header *zhdr)
{
	struct z3fold_buddy_slots *slots;
	int i;
	bool is_free;

	if (WARN_ON(*(unsigned long *)handle == 0))
		return;

	slots = handle_to_slots(handle);
	write_lock(&slots->lock);
	*(unsigned long *)handle = 0;

	if (test_bit(HANDLES_NOFREE, &slots->pool)) {
		write_unlock(&slots->lock);
		return;  
	}

	if (zhdr->slots != slots)
		zhdr->foreign_handles--;

	is_free = true;
	for (i = 0; i <= BUDDY_MASK; i++) {
		if (slots->slot[i]) {
			is_free = false;
			break;
		}
	}
	write_unlock(&slots->lock);

	if (is_free) {
		struct z3fold_pool *pool = slots_to_pool(slots);

		if (zhdr->slots == slots)
			zhdr->slots = NULL;
		kmem_cache_free(pool->c_handle, slots);
	}
}

 
static struct z3fold_header *init_z3fold_page(struct page *page, bool headless,
					struct z3fold_pool *pool, gfp_t gfp)
{
	struct z3fold_header *zhdr = page_address(page);
	struct z3fold_buddy_slots *slots;

	clear_bit(PAGE_HEADLESS, &page->private);
	clear_bit(MIDDLE_CHUNK_MAPPED, &page->private);
	clear_bit(NEEDS_COMPACTING, &page->private);
	clear_bit(PAGE_STALE, &page->private);
	clear_bit(PAGE_CLAIMED, &page->private);
	clear_bit(PAGE_MIGRATED, &page->private);
	if (headless)
		return zhdr;

	slots = alloc_slots(pool, gfp);
	if (!slots)
		return NULL;

	memset(zhdr, 0, sizeof(*zhdr));
	spin_lock_init(&zhdr->page_lock);
	kref_init(&zhdr->refcount);
	zhdr->cpu = -1;
	zhdr->slots = slots;
	zhdr->pool = pool;
	INIT_LIST_HEAD(&zhdr->buddy);
	INIT_WORK(&zhdr->work, compact_page_work);
	return zhdr;
}

 
static void free_z3fold_page(struct page *page, bool headless)
{
	if (!headless) {
		lock_page(page);
		__ClearPageMovable(page);
		unlock_page(page);
	}
	__free_page(page);
}

 
static inline int __idx(struct z3fold_header *zhdr, enum buddy bud)
{
	return (bud + zhdr->first_num) & BUDDY_MASK;
}

 
static unsigned long __encode_handle(struct z3fold_header *zhdr,
				struct z3fold_buddy_slots *slots,
				enum buddy bud)
{
	unsigned long h = (unsigned long)zhdr;
	int idx = 0;

	 
	if (bud == HEADLESS)
		return h | (1 << PAGE_HEADLESS);

	 
	idx = __idx(zhdr, bud);
	h += idx;
	if (bud == LAST)
		h |= (zhdr->last_chunks << BUDDY_SHIFT);

	write_lock(&slots->lock);
	slots->slot[idx] = h;
	write_unlock(&slots->lock);
	return (unsigned long)&slots->slot[idx];
}

static unsigned long encode_handle(struct z3fold_header *zhdr, enum buddy bud)
{
	return __encode_handle(zhdr, zhdr->slots, bud);
}

 
static unsigned short handle_to_chunks(unsigned long handle)
{
	struct z3fold_buddy_slots *slots = handle_to_slots(handle);
	unsigned long addr;

	read_lock(&slots->lock);
	addr = *(unsigned long *)handle;
	read_unlock(&slots->lock);
	return (addr & ~PAGE_MASK) >> BUDDY_SHIFT;
}

 
static enum buddy handle_to_buddy(unsigned long handle)
{
	struct z3fold_header *zhdr;
	struct z3fold_buddy_slots *slots = handle_to_slots(handle);
	unsigned long addr;

	read_lock(&slots->lock);
	WARN_ON(handle & (1 << PAGE_HEADLESS));
	addr = *(unsigned long *)handle;
	read_unlock(&slots->lock);
	zhdr = (struct z3fold_header *)(addr & PAGE_MASK);
	return (addr - zhdr->first_num) & BUDDY_MASK;
}

static inline struct z3fold_pool *zhdr_to_pool(struct z3fold_header *zhdr)
{
	return zhdr->pool;
}

static void __release_z3fold_page(struct z3fold_header *zhdr, bool locked)
{
	struct page *page = virt_to_page(zhdr);
	struct z3fold_pool *pool = zhdr_to_pool(zhdr);

	WARN_ON(!list_empty(&zhdr->buddy));
	set_bit(PAGE_STALE, &page->private);
	clear_bit(NEEDS_COMPACTING, &page->private);
	spin_lock(&pool->lock);
	spin_unlock(&pool->lock);

	if (locked)
		z3fold_page_unlock(zhdr);

	spin_lock(&pool->stale_lock);
	list_add(&zhdr->buddy, &pool->stale);
	queue_work(pool->release_wq, &pool->work);
	spin_unlock(&pool->stale_lock);

	atomic64_dec(&pool->pages_nr);
}

static void release_z3fold_page_locked(struct kref *ref)
{
	struct z3fold_header *zhdr = container_of(ref, struct z3fold_header,
						refcount);
	WARN_ON(z3fold_page_trylock(zhdr));
	__release_z3fold_page(zhdr, true);
}

static void release_z3fold_page_locked_list(struct kref *ref)
{
	struct z3fold_header *zhdr = container_of(ref, struct z3fold_header,
					       refcount);
	struct z3fold_pool *pool = zhdr_to_pool(zhdr);

	spin_lock(&pool->lock);
	list_del_init(&zhdr->buddy);
	spin_unlock(&pool->lock);

	WARN_ON(z3fold_page_trylock(zhdr));
	__release_z3fold_page(zhdr, true);
}

static inline int put_z3fold_locked(struct z3fold_header *zhdr)
{
	return kref_put(&zhdr->refcount, release_z3fold_page_locked);
}

static inline int put_z3fold_locked_list(struct z3fold_header *zhdr)
{
	return kref_put(&zhdr->refcount, release_z3fold_page_locked_list);
}

static void free_pages_work(struct work_struct *w)
{
	struct z3fold_pool *pool = container_of(w, struct z3fold_pool, work);

	spin_lock(&pool->stale_lock);
	while (!list_empty(&pool->stale)) {
		struct z3fold_header *zhdr = list_first_entry(&pool->stale,
						struct z3fold_header, buddy);
		struct page *page = virt_to_page(zhdr);

		list_del(&zhdr->buddy);
		if (WARN_ON(!test_bit(PAGE_STALE, &page->private)))
			continue;
		spin_unlock(&pool->stale_lock);
		cancel_work_sync(&zhdr->work);
		free_z3fold_page(page, false);
		cond_resched();
		spin_lock(&pool->stale_lock);
	}
	spin_unlock(&pool->stale_lock);
}

 
static int num_free_chunks(struct z3fold_header *zhdr)
{
	int nfree;
	 
	if (zhdr->middle_chunks != 0) {
		int nfree_before = zhdr->first_chunks ?
			0 : zhdr->start_middle - ZHDR_CHUNKS;
		int nfree_after = zhdr->last_chunks ?
			0 : TOTAL_CHUNKS -
				(zhdr->start_middle + zhdr->middle_chunks);
		nfree = max(nfree_before, nfree_after);
	} else
		nfree = NCHUNKS - zhdr->first_chunks - zhdr->last_chunks;
	return nfree;
}

 
static inline void add_to_unbuddied(struct z3fold_pool *pool,
				struct z3fold_header *zhdr)
{
	if (zhdr->first_chunks == 0 || zhdr->last_chunks == 0 ||
			zhdr->middle_chunks == 0) {
		struct list_head *unbuddied;
		int freechunks = num_free_chunks(zhdr);

		migrate_disable();
		unbuddied = this_cpu_ptr(pool->unbuddied);
		spin_lock(&pool->lock);
		list_add(&zhdr->buddy, &unbuddied[freechunks]);
		spin_unlock(&pool->lock);
		zhdr->cpu = smp_processor_id();
		migrate_enable();
	}
}

static inline enum buddy get_free_buddy(struct z3fold_header *zhdr, int chunks)
{
	enum buddy bud = HEADLESS;

	if (zhdr->middle_chunks) {
		if (!zhdr->first_chunks &&
		    chunks <= zhdr->start_middle - ZHDR_CHUNKS)
			bud = FIRST;
		else if (!zhdr->last_chunks)
			bud = LAST;
	} else {
		if (!zhdr->first_chunks)
			bud = FIRST;
		else if (!zhdr->last_chunks)
			bud = LAST;
		else
			bud = MIDDLE;
	}

	return bud;
}

static inline void *mchunk_memmove(struct z3fold_header *zhdr,
				unsigned short dst_chunk)
{
	void *beg = zhdr;
	return memmove(beg + (dst_chunk << CHUNK_SHIFT),
		       beg + (zhdr->start_middle << CHUNK_SHIFT),
		       zhdr->middle_chunks << CHUNK_SHIFT);
}

static inline bool buddy_single(struct z3fold_header *zhdr)
{
	return !((zhdr->first_chunks && zhdr->middle_chunks) ||
			(zhdr->first_chunks && zhdr->last_chunks) ||
			(zhdr->middle_chunks && zhdr->last_chunks));
}

static struct z3fold_header *compact_single_buddy(struct z3fold_header *zhdr)
{
	struct z3fold_pool *pool = zhdr_to_pool(zhdr);
	void *p = zhdr;
	unsigned long old_handle = 0;
	size_t sz = 0;
	struct z3fold_header *new_zhdr = NULL;
	int first_idx = __idx(zhdr, FIRST);
	int middle_idx = __idx(zhdr, MIDDLE);
	int last_idx = __idx(zhdr, LAST);
	unsigned short *moved_chunks = NULL;

	 
	if (zhdr->first_chunks && zhdr->slots->slot[first_idx]) {
		p += ZHDR_SIZE_ALIGNED;
		sz = zhdr->first_chunks << CHUNK_SHIFT;
		old_handle = (unsigned long)&zhdr->slots->slot[first_idx];
		moved_chunks = &zhdr->first_chunks;
	} else if (zhdr->middle_chunks && zhdr->slots->slot[middle_idx]) {
		p += zhdr->start_middle << CHUNK_SHIFT;
		sz = zhdr->middle_chunks << CHUNK_SHIFT;
		old_handle = (unsigned long)&zhdr->slots->slot[middle_idx];
		moved_chunks = &zhdr->middle_chunks;
	} else if (zhdr->last_chunks && zhdr->slots->slot[last_idx]) {
		p += PAGE_SIZE - (zhdr->last_chunks << CHUNK_SHIFT);
		sz = zhdr->last_chunks << CHUNK_SHIFT;
		old_handle = (unsigned long)&zhdr->slots->slot[last_idx];
		moved_chunks = &zhdr->last_chunks;
	}

	if (sz > 0) {
		enum buddy new_bud = HEADLESS;
		short chunks = size_to_chunks(sz);
		void *q;

		new_zhdr = __z3fold_alloc(pool, sz, false);
		if (!new_zhdr)
			return NULL;

		if (WARN_ON(new_zhdr == zhdr))
			goto out_fail;

		new_bud = get_free_buddy(new_zhdr, chunks);
		q = new_zhdr;
		switch (new_bud) {
		case FIRST:
			new_zhdr->first_chunks = chunks;
			q += ZHDR_SIZE_ALIGNED;
			break;
		case MIDDLE:
			new_zhdr->middle_chunks = chunks;
			new_zhdr->start_middle =
				new_zhdr->first_chunks + ZHDR_CHUNKS;
			q += new_zhdr->start_middle << CHUNK_SHIFT;
			break;
		case LAST:
			new_zhdr->last_chunks = chunks;
			q += PAGE_SIZE - (new_zhdr->last_chunks << CHUNK_SHIFT);
			break;
		default:
			goto out_fail;
		}
		new_zhdr->foreign_handles++;
		memcpy(q, p, sz);
		write_lock(&zhdr->slots->lock);
		*(unsigned long *)old_handle = (unsigned long)new_zhdr +
			__idx(new_zhdr, new_bud);
		if (new_bud == LAST)
			*(unsigned long *)old_handle |=
					(new_zhdr->last_chunks << BUDDY_SHIFT);
		write_unlock(&zhdr->slots->lock);
		add_to_unbuddied(pool, new_zhdr);
		z3fold_page_unlock(new_zhdr);

		*moved_chunks = 0;
	}

	return new_zhdr;

out_fail:
	if (new_zhdr && !put_z3fold_locked(new_zhdr)) {
		add_to_unbuddied(pool, new_zhdr);
		z3fold_page_unlock(new_zhdr);
	}
	return NULL;

}

#define BIG_CHUNK_GAP	3
 
static int z3fold_compact_page(struct z3fold_header *zhdr)
{
	struct page *page = virt_to_page(zhdr);

	if (test_bit(MIDDLE_CHUNK_MAPPED, &page->private))
		return 0;  

	if (unlikely(PageIsolated(page)))
		return 0;

	if (zhdr->middle_chunks == 0)
		return 0;  

	if (zhdr->first_chunks == 0 && zhdr->last_chunks == 0) {
		 
		mchunk_memmove(zhdr, ZHDR_CHUNKS);
		zhdr->first_chunks = zhdr->middle_chunks;
		zhdr->middle_chunks = 0;
		zhdr->start_middle = 0;
		zhdr->first_num++;
		return 1;
	}

	 
	if (zhdr->first_chunks != 0 && zhdr->last_chunks == 0 &&
	    zhdr->start_middle - (zhdr->first_chunks + ZHDR_CHUNKS) >=
			BIG_CHUNK_GAP) {
		mchunk_memmove(zhdr, zhdr->first_chunks + ZHDR_CHUNKS);
		zhdr->start_middle = zhdr->first_chunks + ZHDR_CHUNKS;
		return 1;
	} else if (zhdr->last_chunks != 0 && zhdr->first_chunks == 0 &&
		   TOTAL_CHUNKS - (zhdr->last_chunks + zhdr->start_middle
					+ zhdr->middle_chunks) >=
			BIG_CHUNK_GAP) {
		unsigned short new_start = TOTAL_CHUNKS - zhdr->last_chunks -
			zhdr->middle_chunks;
		mchunk_memmove(zhdr, new_start);
		zhdr->start_middle = new_start;
		return 1;
	}

	return 0;
}

static void do_compact_page(struct z3fold_header *zhdr, bool locked)
{
	struct z3fold_pool *pool = zhdr_to_pool(zhdr);
	struct page *page;

	page = virt_to_page(zhdr);
	if (locked)
		WARN_ON(z3fold_page_trylock(zhdr));
	else
		z3fold_page_lock(zhdr);
	if (WARN_ON(!test_and_clear_bit(NEEDS_COMPACTING, &page->private))) {
		z3fold_page_unlock(zhdr);
		return;
	}
	spin_lock(&pool->lock);
	list_del_init(&zhdr->buddy);
	spin_unlock(&pool->lock);

	if (put_z3fold_locked(zhdr))
		return;

	if (test_bit(PAGE_STALE, &page->private) ||
	    test_and_set_bit(PAGE_CLAIMED, &page->private)) {
		z3fold_page_unlock(zhdr);
		return;
	}

	if (!zhdr->foreign_handles && buddy_single(zhdr) &&
	    zhdr->mapped_count == 0 && compact_single_buddy(zhdr)) {
		if (!put_z3fold_locked(zhdr)) {
			clear_bit(PAGE_CLAIMED, &page->private);
			z3fold_page_unlock(zhdr);
		}
		return;
	}

	z3fold_compact_page(zhdr);
	add_to_unbuddied(pool, zhdr);
	clear_bit(PAGE_CLAIMED, &page->private);
	z3fold_page_unlock(zhdr);
}

static void compact_page_work(struct work_struct *w)
{
	struct z3fold_header *zhdr = container_of(w, struct z3fold_header,
						work);

	do_compact_page(zhdr, false);
}

 
static inline struct z3fold_header *__z3fold_alloc(struct z3fold_pool *pool,
						size_t size, bool can_sleep)
{
	struct z3fold_header *zhdr = NULL;
	struct page *page;
	struct list_head *unbuddied;
	int chunks = size_to_chunks(size), i;

lookup:
	migrate_disable();
	 
	unbuddied = this_cpu_ptr(pool->unbuddied);
	for_each_unbuddied_list(i, chunks) {
		struct list_head *l = &unbuddied[i];

		zhdr = list_first_entry_or_null(READ_ONCE(l),
					struct z3fold_header, buddy);

		if (!zhdr)
			continue;

		 
		spin_lock(&pool->lock);
		if (unlikely(zhdr != list_first_entry(READ_ONCE(l),
						struct z3fold_header, buddy)) ||
		    !z3fold_page_trylock(zhdr)) {
			spin_unlock(&pool->lock);
			zhdr = NULL;
			migrate_enable();
			if (can_sleep)
				cond_resched();
			goto lookup;
		}
		list_del_init(&zhdr->buddy);
		zhdr->cpu = -1;
		spin_unlock(&pool->lock);

		page = virt_to_page(zhdr);
		if (test_bit(NEEDS_COMPACTING, &page->private) ||
		    test_bit(PAGE_CLAIMED, &page->private)) {
			z3fold_page_unlock(zhdr);
			zhdr = NULL;
			migrate_enable();
			if (can_sleep)
				cond_resched();
			goto lookup;
		}

		 
		kref_get(&zhdr->refcount);
		break;
	}
	migrate_enable();

	if (!zhdr) {
		int cpu;

		 
		for_each_online_cpu(cpu) {
			struct list_head *l;

			unbuddied = per_cpu_ptr(pool->unbuddied, cpu);
			spin_lock(&pool->lock);
			l = &unbuddied[chunks];

			zhdr = list_first_entry_or_null(READ_ONCE(l),
						struct z3fold_header, buddy);

			if (!zhdr || !z3fold_page_trylock(zhdr)) {
				spin_unlock(&pool->lock);
				zhdr = NULL;
				continue;
			}
			list_del_init(&zhdr->buddy);
			zhdr->cpu = -1;
			spin_unlock(&pool->lock);

			page = virt_to_page(zhdr);
			if (test_bit(NEEDS_COMPACTING, &page->private) ||
			    test_bit(PAGE_CLAIMED, &page->private)) {
				z3fold_page_unlock(zhdr);
				zhdr = NULL;
				if (can_sleep)
					cond_resched();
				continue;
			}
			kref_get(&zhdr->refcount);
			break;
		}
	}

	if (zhdr && !zhdr->slots) {
		zhdr->slots = alloc_slots(pool, GFP_ATOMIC);
		if (!zhdr->slots)
			goto out_fail;
	}
	return zhdr;

out_fail:
	if (!put_z3fold_locked(zhdr)) {
		add_to_unbuddied(pool, zhdr);
		z3fold_page_unlock(zhdr);
	}
	return NULL;
}

 

 
static struct z3fold_pool *z3fold_create_pool(const char *name, gfp_t gfp)
{
	struct z3fold_pool *pool = NULL;
	int i, cpu;

	pool = kzalloc(sizeof(struct z3fold_pool), gfp);
	if (!pool)
		goto out;
	pool->c_handle = kmem_cache_create("z3fold_handle",
				sizeof(struct z3fold_buddy_slots),
				SLOTS_ALIGN, 0, NULL);
	if (!pool->c_handle)
		goto out_c;
	spin_lock_init(&pool->lock);
	spin_lock_init(&pool->stale_lock);
	pool->unbuddied = __alloc_percpu(sizeof(struct list_head) * NCHUNKS,
					 __alignof__(struct list_head));
	if (!pool->unbuddied)
		goto out_pool;
	for_each_possible_cpu(cpu) {
		struct list_head *unbuddied =
				per_cpu_ptr(pool->unbuddied, cpu);
		for_each_unbuddied_list(i, 0)
			INIT_LIST_HEAD(&unbuddied[i]);
	}
	INIT_LIST_HEAD(&pool->stale);
	atomic64_set(&pool->pages_nr, 0);
	pool->name = name;
	pool->compact_wq = create_singlethread_workqueue(pool->name);
	if (!pool->compact_wq)
		goto out_unbuddied;
	pool->release_wq = create_singlethread_workqueue(pool->name);
	if (!pool->release_wq)
		goto out_wq;
	INIT_WORK(&pool->work, free_pages_work);
	return pool;

out_wq:
	destroy_workqueue(pool->compact_wq);
out_unbuddied:
	free_percpu(pool->unbuddied);
out_pool:
	kmem_cache_destroy(pool->c_handle);
out_c:
	kfree(pool);
out:
	return NULL;
}

 
static void z3fold_destroy_pool(struct z3fold_pool *pool)
{
	kmem_cache_destroy(pool->c_handle);

	 

	destroy_workqueue(pool->compact_wq);
	destroy_workqueue(pool->release_wq);
	free_percpu(pool->unbuddied);
	kfree(pool);
}

static const struct movable_operations z3fold_mops;

 
static int z3fold_alloc(struct z3fold_pool *pool, size_t size, gfp_t gfp,
			unsigned long *handle)
{
	int chunks = size_to_chunks(size);
	struct z3fold_header *zhdr = NULL;
	struct page *page = NULL;
	enum buddy bud;
	bool can_sleep = gfpflags_allow_blocking(gfp);

	if (!size || (gfp & __GFP_HIGHMEM))
		return -EINVAL;

	if (size > PAGE_SIZE)
		return -ENOSPC;

	if (size > PAGE_SIZE - ZHDR_SIZE_ALIGNED - CHUNK_SIZE)
		bud = HEADLESS;
	else {
retry:
		zhdr = __z3fold_alloc(pool, size, can_sleep);
		if (zhdr) {
			bud = get_free_buddy(zhdr, chunks);
			if (bud == HEADLESS) {
				if (!put_z3fold_locked(zhdr))
					z3fold_page_unlock(zhdr);
				pr_err("No free chunks in unbuddied\n");
				WARN_ON(1);
				goto retry;
			}
			page = virt_to_page(zhdr);
			goto found;
		}
		bud = FIRST;
	}

	page = alloc_page(gfp);
	if (!page)
		return -ENOMEM;

	zhdr = init_z3fold_page(page, bud == HEADLESS, pool, gfp);
	if (!zhdr) {
		__free_page(page);
		return -ENOMEM;
	}
	atomic64_inc(&pool->pages_nr);

	if (bud == HEADLESS) {
		set_bit(PAGE_HEADLESS, &page->private);
		goto headless;
	}
	if (can_sleep) {
		lock_page(page);
		__SetPageMovable(page, &z3fold_mops);
		unlock_page(page);
	} else {
		WARN_ON(!trylock_page(page));
		__SetPageMovable(page, &z3fold_mops);
		unlock_page(page);
	}
	z3fold_page_lock(zhdr);

found:
	if (bud == FIRST)
		zhdr->first_chunks = chunks;
	else if (bud == LAST)
		zhdr->last_chunks = chunks;
	else {
		zhdr->middle_chunks = chunks;
		zhdr->start_middle = zhdr->first_chunks + ZHDR_CHUNKS;
	}
	add_to_unbuddied(pool, zhdr);

headless:
	spin_lock(&pool->lock);
	*handle = encode_handle(zhdr, bud);
	spin_unlock(&pool->lock);
	if (bud != HEADLESS)
		z3fold_page_unlock(zhdr);

	return 0;
}

 
static void z3fold_free(struct z3fold_pool *pool, unsigned long handle)
{
	struct z3fold_header *zhdr;
	struct page *page;
	enum buddy bud;
	bool page_claimed;

	zhdr = get_z3fold_header(handle);
	page = virt_to_page(zhdr);
	page_claimed = test_and_set_bit(PAGE_CLAIMED, &page->private);

	if (test_bit(PAGE_HEADLESS, &page->private)) {
		 
		if (!page_claimed) {
			put_z3fold_header(zhdr);
			free_z3fold_page(page, true);
			atomic64_dec(&pool->pages_nr);
		}
		return;
	}

	 
	bud = handle_to_buddy(handle);

	switch (bud) {
	case FIRST:
		zhdr->first_chunks = 0;
		break;
	case MIDDLE:
		zhdr->middle_chunks = 0;
		break;
	case LAST:
		zhdr->last_chunks = 0;
		break;
	default:
		pr_err("%s: unknown bud %d\n", __func__, bud);
		WARN_ON(1);
		put_z3fold_header(zhdr);
		return;
	}

	if (!page_claimed)
		free_handle(handle, zhdr);
	if (put_z3fold_locked_list(zhdr))
		return;
	if (page_claimed) {
		 
		put_z3fold_header(zhdr);
		return;
	}
	if (test_and_set_bit(NEEDS_COMPACTING, &page->private)) {
		clear_bit(PAGE_CLAIMED, &page->private);
		put_z3fold_header(zhdr);
		return;
	}
	if (zhdr->cpu < 0 || !cpu_online(zhdr->cpu)) {
		zhdr->cpu = -1;
		kref_get(&zhdr->refcount);
		clear_bit(PAGE_CLAIMED, &page->private);
		do_compact_page(zhdr, true);
		return;
	}
	kref_get(&zhdr->refcount);
	clear_bit(PAGE_CLAIMED, &page->private);
	queue_work_on(zhdr->cpu, pool->compact_wq, &zhdr->work);
	put_z3fold_header(zhdr);
}

 
static void *z3fold_map(struct z3fold_pool *pool, unsigned long handle)
{
	struct z3fold_header *zhdr;
	struct page *page;
	void *addr;
	enum buddy buddy;

	zhdr = get_z3fold_header(handle);
	addr = zhdr;
	page = virt_to_page(zhdr);

	if (test_bit(PAGE_HEADLESS, &page->private))
		goto out;

	buddy = handle_to_buddy(handle);
	switch (buddy) {
	case FIRST:
		addr += ZHDR_SIZE_ALIGNED;
		break;
	case MIDDLE:
		addr += zhdr->start_middle << CHUNK_SHIFT;
		set_bit(MIDDLE_CHUNK_MAPPED, &page->private);
		break;
	case LAST:
		addr += PAGE_SIZE - (handle_to_chunks(handle) << CHUNK_SHIFT);
		break;
	default:
		pr_err("unknown buddy id %d\n", buddy);
		WARN_ON(1);
		addr = NULL;
		break;
	}

	if (addr)
		zhdr->mapped_count++;
out:
	put_z3fold_header(zhdr);
	return addr;
}

 
static void z3fold_unmap(struct z3fold_pool *pool, unsigned long handle)
{
	struct z3fold_header *zhdr;
	struct page *page;
	enum buddy buddy;

	zhdr = get_z3fold_header(handle);
	page = virt_to_page(zhdr);

	if (test_bit(PAGE_HEADLESS, &page->private))
		return;

	buddy = handle_to_buddy(handle);
	if (buddy == MIDDLE)
		clear_bit(MIDDLE_CHUNK_MAPPED, &page->private);
	zhdr->mapped_count--;
	put_z3fold_header(zhdr);
}

 
static u64 z3fold_get_pool_size(struct z3fold_pool *pool)
{
	return atomic64_read(&pool->pages_nr);
}

static bool z3fold_page_isolate(struct page *page, isolate_mode_t mode)
{
	struct z3fold_header *zhdr;
	struct z3fold_pool *pool;

	VM_BUG_ON_PAGE(PageIsolated(page), page);

	if (test_bit(PAGE_HEADLESS, &page->private))
		return false;

	zhdr = page_address(page);
	z3fold_page_lock(zhdr);
	if (test_bit(NEEDS_COMPACTING, &page->private) ||
	    test_bit(PAGE_STALE, &page->private))
		goto out;

	if (zhdr->mapped_count != 0 || zhdr->foreign_handles != 0)
		goto out;

	if (test_and_set_bit(PAGE_CLAIMED, &page->private))
		goto out;
	pool = zhdr_to_pool(zhdr);
	spin_lock(&pool->lock);
	if (!list_empty(&zhdr->buddy))
		list_del_init(&zhdr->buddy);
	spin_unlock(&pool->lock);

	kref_get(&zhdr->refcount);
	z3fold_page_unlock(zhdr);
	return true;

out:
	z3fold_page_unlock(zhdr);
	return false;
}

static int z3fold_page_migrate(struct page *newpage, struct page *page,
		enum migrate_mode mode)
{
	struct z3fold_header *zhdr, *new_zhdr;
	struct z3fold_pool *pool;

	VM_BUG_ON_PAGE(!PageIsolated(page), page);
	VM_BUG_ON_PAGE(!test_bit(PAGE_CLAIMED, &page->private), page);
	VM_BUG_ON_PAGE(!PageLocked(newpage), newpage);

	zhdr = page_address(page);
	pool = zhdr_to_pool(zhdr);

	if (!z3fold_page_trylock(zhdr))
		return -EAGAIN;
	if (zhdr->mapped_count != 0 || zhdr->foreign_handles != 0) {
		clear_bit(PAGE_CLAIMED, &page->private);
		z3fold_page_unlock(zhdr);
		return -EBUSY;
	}
	if (work_pending(&zhdr->work)) {
		z3fold_page_unlock(zhdr);
		return -EAGAIN;
	}
	new_zhdr = page_address(newpage);
	memcpy(new_zhdr, zhdr, PAGE_SIZE);
	newpage->private = page->private;
	set_bit(PAGE_MIGRATED, &page->private);
	z3fold_page_unlock(zhdr);
	spin_lock_init(&new_zhdr->page_lock);
	INIT_WORK(&new_zhdr->work, compact_page_work);
	 
	INIT_LIST_HEAD(&new_zhdr->buddy);
	__ClearPageMovable(page);

	get_page(newpage);
	z3fold_page_lock(new_zhdr);
	if (new_zhdr->first_chunks)
		encode_handle(new_zhdr, FIRST);
	if (new_zhdr->last_chunks)
		encode_handle(new_zhdr, LAST);
	if (new_zhdr->middle_chunks)
		encode_handle(new_zhdr, MIDDLE);
	set_bit(NEEDS_COMPACTING, &newpage->private);
	new_zhdr->cpu = smp_processor_id();
	__SetPageMovable(newpage, &z3fold_mops);
	z3fold_page_unlock(new_zhdr);

	queue_work_on(new_zhdr->cpu, pool->compact_wq, &new_zhdr->work);

	 
	page->private = 0;
	put_page(page);
	return 0;
}

static void z3fold_page_putback(struct page *page)
{
	struct z3fold_header *zhdr;
	struct z3fold_pool *pool;

	zhdr = page_address(page);
	pool = zhdr_to_pool(zhdr);

	z3fold_page_lock(zhdr);
	if (!list_empty(&zhdr->buddy))
		list_del_init(&zhdr->buddy);
	INIT_LIST_HEAD(&page->lru);
	if (put_z3fold_locked(zhdr))
		return;
	if (list_empty(&zhdr->buddy))
		add_to_unbuddied(pool, zhdr);
	clear_bit(PAGE_CLAIMED, &page->private);
	z3fold_page_unlock(zhdr);
}

static const struct movable_operations z3fold_mops = {
	.isolate_page = z3fold_page_isolate,
	.migrate_page = z3fold_page_migrate,
	.putback_page = z3fold_page_putback,
};

 

static void *z3fold_zpool_create(const char *name, gfp_t gfp)
{
	return z3fold_create_pool(name, gfp);
}

static void z3fold_zpool_destroy(void *pool)
{
	z3fold_destroy_pool(pool);
}

static int z3fold_zpool_malloc(void *pool, size_t size, gfp_t gfp,
			unsigned long *handle)
{
	return z3fold_alloc(pool, size, gfp, handle);
}
static void z3fold_zpool_free(void *pool, unsigned long handle)
{
	z3fold_free(pool, handle);
}

static void *z3fold_zpool_map(void *pool, unsigned long handle,
			enum zpool_mapmode mm)
{
	return z3fold_map(pool, handle);
}
static void z3fold_zpool_unmap(void *pool, unsigned long handle)
{
	z3fold_unmap(pool, handle);
}

static u64 z3fold_zpool_total_size(void *pool)
{
	return z3fold_get_pool_size(pool) * PAGE_SIZE;
}

static struct zpool_driver z3fold_zpool_driver = {
	.type =		"z3fold",
	.sleep_mapped = true,
	.owner =	THIS_MODULE,
	.create =	z3fold_zpool_create,
	.destroy =	z3fold_zpool_destroy,
	.malloc =	z3fold_zpool_malloc,
	.free =		z3fold_zpool_free,
	.map =		z3fold_zpool_map,
	.unmap =	z3fold_zpool_unmap,
	.total_size =	z3fold_zpool_total_size,
};

MODULE_ALIAS("zpool-z3fold");

static int __init init_z3fold(void)
{
	 
	BUILD_BUG_ON(ZHDR_SIZE_ALIGNED > PAGE_SIZE - CHUNK_SIZE);
	zpool_register_driver(&z3fold_zpool_driver);

	return 0;
}

static void __exit exit_z3fold(void)
{
	zpool_unregister_driver(&z3fold_zpool_driver);
}

module_init(init_z3fold);
module_exit(exit_z3fold);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Vitaly Wool <vitalywool@gmail.com>");
MODULE_DESCRIPTION("3-Fold Allocator for Compressed Pages");
