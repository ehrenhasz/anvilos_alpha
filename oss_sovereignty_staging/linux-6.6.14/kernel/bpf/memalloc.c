
 
#include <linux/mm.h>
#include <linux/llist.h>
#include <linux/bpf.h>
#include <linux/irq_work.h>
#include <linux/bpf_mem_alloc.h>
#include <linux/memcontrol.h>
#include <asm/local.h>

 
#define LLIST_NODE_SZ sizeof(struct llist_node)

 
static u8 size_index[24] __ro_after_init = {
	3,	 
	3,	 
	4,	 
	4,	 
	5,	 
	5,	 
	5,	 
	5,	 
	1,	 
	1,	 
	1,	 
	1,	 
	6,	 
	6,	 
	6,	 
	6,	 
	2,	 
	2,	 
	2,	 
	2,	 
	2,	 
	2,	 
	2,	 
	2	 
};

static int bpf_mem_cache_idx(size_t size)
{
	if (!size || size > 4096)
		return -1;

	if (size <= 192)
		return size_index[(size - 1) / 8] - 1;

	return fls(size - 1) - 2;
}

#define NUM_CACHES 11

struct bpf_mem_cache {
	 
	struct llist_head free_llist;
	local_t active;

	 
	struct llist_head free_llist_extra;

	struct irq_work refill_work;
	struct obj_cgroup *objcg;
	int unit_size;
	 
	int free_cnt;
	int low_watermark, high_watermark, batch;
	int percpu_size;
	bool draining;
	struct bpf_mem_cache *tgt;

	 
	struct llist_head free_by_rcu;
	struct llist_node *free_by_rcu_tail;
	struct llist_head waiting_for_gp;
	struct llist_node *waiting_for_gp_tail;
	struct rcu_head rcu;
	atomic_t call_rcu_in_progress;
	struct llist_head free_llist_extra_rcu;

	 
	struct llist_head free_by_rcu_ttrace;
	struct llist_head waiting_for_gp_ttrace;
	struct rcu_head rcu_ttrace;
	atomic_t call_rcu_ttrace_in_progress;
};

struct bpf_mem_caches {
	struct bpf_mem_cache cache[NUM_CACHES];
};

static struct llist_node notrace *__llist_del_first(struct llist_head *head)
{
	struct llist_node *entry, *next;

	entry = head->first;
	if (!entry)
		return NULL;
	next = entry->next;
	head->first = next;
	return entry;
}

static void *__alloc(struct bpf_mem_cache *c, int node, gfp_t flags)
{
	if (c->percpu_size) {
		void **obj = kmalloc_node(c->percpu_size, flags, node);
		void *pptr = __alloc_percpu_gfp(c->unit_size, 8, flags);

		if (!obj || !pptr) {
			free_percpu(pptr);
			kfree(obj);
			return NULL;
		}
		obj[1] = pptr;
		return obj;
	}

	return kmalloc_node(c->unit_size, flags | __GFP_ZERO, node);
}

static struct mem_cgroup *get_memcg(const struct bpf_mem_cache *c)
{
#ifdef CONFIG_MEMCG_KMEM
	if (c->objcg)
		return get_mem_cgroup_from_objcg(c->objcg);
#endif

#ifdef CONFIG_MEMCG
	return root_mem_cgroup;
#else
	return NULL;
#endif
}

static void inc_active(struct bpf_mem_cache *c, unsigned long *flags)
{
	if (IS_ENABLED(CONFIG_PREEMPT_RT))
		 
		local_irq_save(*flags);
	 
	WARN_ON_ONCE(local_inc_return(&c->active) != 1);
}

static void dec_active(struct bpf_mem_cache *c, unsigned long *flags)
{
	local_dec(&c->active);
	if (IS_ENABLED(CONFIG_PREEMPT_RT))
		local_irq_restore(*flags);
}

static void add_obj_to_free_list(struct bpf_mem_cache *c, void *obj)
{
	unsigned long flags;

	inc_active(c, &flags);
	__llist_add(obj, &c->free_llist);
	c->free_cnt++;
	dec_active(c, &flags);
}

 
static void alloc_bulk(struct bpf_mem_cache *c, int cnt, int node, bool atomic)
{
	struct mem_cgroup *memcg = NULL, *old_memcg;
	gfp_t gfp;
	void *obj;
	int i;

	gfp = __GFP_NOWARN | __GFP_ACCOUNT;
	gfp |= atomic ? GFP_NOWAIT : GFP_KERNEL;

	for (i = 0; i < cnt; i++) {
		 
		obj = llist_del_first(&c->free_by_rcu_ttrace);
		if (!obj)
			break;
		add_obj_to_free_list(c, obj);
	}
	if (i >= cnt)
		return;

	for (; i < cnt; i++) {
		obj = llist_del_first(&c->waiting_for_gp_ttrace);
		if (!obj)
			break;
		add_obj_to_free_list(c, obj);
	}
	if (i >= cnt)
		return;

	memcg = get_memcg(c);
	old_memcg = set_active_memcg(memcg);
	for (; i < cnt; i++) {
		 
		obj = __alloc(c, node, gfp);
		if (!obj)
			break;
		add_obj_to_free_list(c, obj);
	}
	set_active_memcg(old_memcg);
	mem_cgroup_put(memcg);
}

static void free_one(void *obj, bool percpu)
{
	if (percpu) {
		free_percpu(((void **)obj)[1]);
		kfree(obj);
		return;
	}

	kfree(obj);
}

static int free_all(struct llist_node *llnode, bool percpu)
{
	struct llist_node *pos, *t;
	int cnt = 0;

	llist_for_each_safe(pos, t, llnode) {
		free_one(pos, percpu);
		cnt++;
	}
	return cnt;
}

static void __free_rcu(struct rcu_head *head)
{
	struct bpf_mem_cache *c = container_of(head, struct bpf_mem_cache, rcu_ttrace);

	free_all(llist_del_all(&c->waiting_for_gp_ttrace), !!c->percpu_size);
	atomic_set(&c->call_rcu_ttrace_in_progress, 0);
}

static void __free_rcu_tasks_trace(struct rcu_head *head)
{
	 
	if (rcu_trace_implies_rcu_gp())
		__free_rcu(head);
	else
		call_rcu(head, __free_rcu);
}

static void enque_to_free(struct bpf_mem_cache *c, void *obj)
{
	struct llist_node *llnode = obj;

	 
	llist_add(llnode, &c->free_by_rcu_ttrace);
}

static void do_call_rcu_ttrace(struct bpf_mem_cache *c)
{
	struct llist_node *llnode, *t;

	if (atomic_xchg(&c->call_rcu_ttrace_in_progress, 1)) {
		if (unlikely(READ_ONCE(c->draining))) {
			llnode = llist_del_all(&c->free_by_rcu_ttrace);
			free_all(llnode, !!c->percpu_size);
		}
		return;
	}

	WARN_ON_ONCE(!llist_empty(&c->waiting_for_gp_ttrace));
	llist_for_each_safe(llnode, t, llist_del_all(&c->free_by_rcu_ttrace))
		llist_add(llnode, &c->waiting_for_gp_ttrace);

	if (unlikely(READ_ONCE(c->draining))) {
		__free_rcu(&c->rcu_ttrace);
		return;
	}

	 
	call_rcu_tasks_trace(&c->rcu_ttrace, __free_rcu_tasks_trace);
}

static void free_bulk(struct bpf_mem_cache *c)
{
	struct bpf_mem_cache *tgt = c->tgt;
	struct llist_node *llnode, *t;
	unsigned long flags;
	int cnt;

	WARN_ON_ONCE(tgt->unit_size != c->unit_size);

	do {
		inc_active(c, &flags);
		llnode = __llist_del_first(&c->free_llist);
		if (llnode)
			cnt = --c->free_cnt;
		else
			cnt = 0;
		dec_active(c, &flags);
		if (llnode)
			enque_to_free(tgt, llnode);
	} while (cnt > (c->high_watermark + c->low_watermark) / 2);

	 
	llist_for_each_safe(llnode, t, llist_del_all(&c->free_llist_extra))
		enque_to_free(tgt, llnode);
	do_call_rcu_ttrace(tgt);
}

static void __free_by_rcu(struct rcu_head *head)
{
	struct bpf_mem_cache *c = container_of(head, struct bpf_mem_cache, rcu);
	struct bpf_mem_cache *tgt = c->tgt;
	struct llist_node *llnode;

	llnode = llist_del_all(&c->waiting_for_gp);
	if (!llnode)
		goto out;

	llist_add_batch(llnode, c->waiting_for_gp_tail, &tgt->free_by_rcu_ttrace);

	 
	do_call_rcu_ttrace(tgt);
out:
	atomic_set(&c->call_rcu_in_progress, 0);
}

static void check_free_by_rcu(struct bpf_mem_cache *c)
{
	struct llist_node *llnode, *t;
	unsigned long flags;

	 
	if (unlikely(!llist_empty(&c->free_llist_extra_rcu))) {
		inc_active(c, &flags);
		llist_for_each_safe(llnode, t, llist_del_all(&c->free_llist_extra_rcu))
			if (__llist_add(llnode, &c->free_by_rcu))
				c->free_by_rcu_tail = llnode;
		dec_active(c, &flags);
	}

	if (llist_empty(&c->free_by_rcu))
		return;

	if (atomic_xchg(&c->call_rcu_in_progress, 1)) {
		 
		rcu_request_urgent_qs_task(current);
		return;
	}

	WARN_ON_ONCE(!llist_empty(&c->waiting_for_gp));

	inc_active(c, &flags);
	WRITE_ONCE(c->waiting_for_gp.first, __llist_del_all(&c->free_by_rcu));
	c->waiting_for_gp_tail = c->free_by_rcu_tail;
	dec_active(c, &flags);

	if (unlikely(READ_ONCE(c->draining))) {
		free_all(llist_del_all(&c->waiting_for_gp), !!c->percpu_size);
		atomic_set(&c->call_rcu_in_progress, 0);
	} else {
		call_rcu_hurry(&c->rcu, __free_by_rcu);
	}
}

static void bpf_mem_refill(struct irq_work *work)
{
	struct bpf_mem_cache *c = container_of(work, struct bpf_mem_cache, refill_work);
	int cnt;

	 
	cnt = c->free_cnt;
	if (cnt < c->low_watermark)
		 
		alloc_bulk(c, c->batch, NUMA_NO_NODE, true);
	else if (cnt > c->high_watermark)
		free_bulk(c);

	check_free_by_rcu(c);
}

static void notrace irq_work_raise(struct bpf_mem_cache *c)
{
	irq_work_queue(&c->refill_work);
}

 
static void init_refill_work(struct bpf_mem_cache *c)
{
	init_irq_work(&c->refill_work, bpf_mem_refill);
	if (c->unit_size <= 256) {
		c->low_watermark = 32;
		c->high_watermark = 96;
	} else {
		 
		c->low_watermark = max(32 * 256 / c->unit_size, 1);
		c->high_watermark = max(96 * 256 / c->unit_size, 3);
	}
	c->batch = max((c->high_watermark - c->low_watermark) / 4 * 3, 1);
}

static void prefill_mem_cache(struct bpf_mem_cache *c, int cpu)
{
	 
	alloc_bulk(c, c->unit_size <= 256 ? 4 : 1, cpu_to_node(cpu), false);
}

 
int bpf_mem_alloc_init(struct bpf_mem_alloc *ma, int size, bool percpu)
{
	static u16 sizes[NUM_CACHES] = {96, 192, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096};
	struct bpf_mem_caches *cc, __percpu *pcc;
	struct bpf_mem_cache *c, __percpu *pc;
	struct obj_cgroup *objcg = NULL;
	int cpu, i, unit_size, percpu_size = 0;

	ma->percpu = percpu;

	if (size) {
		pc = __alloc_percpu_gfp(sizeof(*pc), 8, GFP_KERNEL);
		if (!pc)
			return -ENOMEM;

		if (percpu)
			 
			percpu_size = LLIST_NODE_SZ + sizeof(void *);
		else
			size += LLIST_NODE_SZ;  
		unit_size = size;

#ifdef CONFIG_MEMCG_KMEM
		if (memcg_bpf_enabled())
			objcg = get_obj_cgroup_from_current();
#endif
		for_each_possible_cpu(cpu) {
			c = per_cpu_ptr(pc, cpu);
			c->unit_size = unit_size;
			c->objcg = objcg;
			c->percpu_size = percpu_size;
			c->tgt = c;
			init_refill_work(c);
			prefill_mem_cache(c, cpu);
		}
		ma->cache = pc;
		return 0;
	}

	 
	if (WARN_ON_ONCE(percpu))
		return -EINVAL;

	pcc = __alloc_percpu_gfp(sizeof(*cc), 8, GFP_KERNEL);
	if (!pcc)
		return -ENOMEM;
#ifdef CONFIG_MEMCG_KMEM
	objcg = get_obj_cgroup_from_current();
#endif
	for_each_possible_cpu(cpu) {
		cc = per_cpu_ptr(pcc, cpu);
		for (i = 0; i < NUM_CACHES; i++) {
			c = &cc->cache[i];
			c->unit_size = sizes[i];
			c->objcg = objcg;
			c->tgt = c;

			init_refill_work(c);
			prefill_mem_cache(c, cpu);
		}
	}

	ma->caches = pcc;
	return 0;
}

static void drain_mem_cache(struct bpf_mem_cache *c)
{
	bool percpu = !!c->percpu_size;

	 
	free_all(llist_del_all(&c->free_by_rcu_ttrace), percpu);
	free_all(llist_del_all(&c->waiting_for_gp_ttrace), percpu);
	free_all(__llist_del_all(&c->free_llist), percpu);
	free_all(__llist_del_all(&c->free_llist_extra), percpu);
	free_all(__llist_del_all(&c->free_by_rcu), percpu);
	free_all(__llist_del_all(&c->free_llist_extra_rcu), percpu);
	free_all(llist_del_all(&c->waiting_for_gp), percpu);
}

static void check_mem_cache(struct bpf_mem_cache *c)
{
	WARN_ON_ONCE(!llist_empty(&c->free_by_rcu_ttrace));
	WARN_ON_ONCE(!llist_empty(&c->waiting_for_gp_ttrace));
	WARN_ON_ONCE(!llist_empty(&c->free_llist));
	WARN_ON_ONCE(!llist_empty(&c->free_llist_extra));
	WARN_ON_ONCE(!llist_empty(&c->free_by_rcu));
	WARN_ON_ONCE(!llist_empty(&c->free_llist_extra_rcu));
	WARN_ON_ONCE(!llist_empty(&c->waiting_for_gp));
}

static void check_leaked_objs(struct bpf_mem_alloc *ma)
{
	struct bpf_mem_caches *cc;
	struct bpf_mem_cache *c;
	int cpu, i;

	if (ma->cache) {
		for_each_possible_cpu(cpu) {
			c = per_cpu_ptr(ma->cache, cpu);
			check_mem_cache(c);
		}
	}
	if (ma->caches) {
		for_each_possible_cpu(cpu) {
			cc = per_cpu_ptr(ma->caches, cpu);
			for (i = 0; i < NUM_CACHES; i++) {
				c = &cc->cache[i];
				check_mem_cache(c);
			}
		}
	}
}

static void free_mem_alloc_no_barrier(struct bpf_mem_alloc *ma)
{
	check_leaked_objs(ma);
	free_percpu(ma->cache);
	free_percpu(ma->caches);
	ma->cache = NULL;
	ma->caches = NULL;
}

static void free_mem_alloc(struct bpf_mem_alloc *ma)
{
	 
	rcu_barrier();  
	rcu_barrier_tasks_trace();  
	if (!rcu_trace_implies_rcu_gp())
		rcu_barrier();
	free_mem_alloc_no_barrier(ma);
}

static void free_mem_alloc_deferred(struct work_struct *work)
{
	struct bpf_mem_alloc *ma = container_of(work, struct bpf_mem_alloc, work);

	free_mem_alloc(ma);
	kfree(ma);
}

static void destroy_mem_alloc(struct bpf_mem_alloc *ma, int rcu_in_progress)
{
	struct bpf_mem_alloc *copy;

	if (!rcu_in_progress) {
		 
		free_mem_alloc_no_barrier(ma);
		return;
	}

	copy = kmemdup(ma, sizeof(*ma), GFP_KERNEL);
	if (!copy) {
		 
		free_mem_alloc(ma);
		return;
	}

	 
	memset(ma, 0, sizeof(*ma));
	INIT_WORK(&copy->work, free_mem_alloc_deferred);
	queue_work(system_unbound_wq, &copy->work);
}

void bpf_mem_alloc_destroy(struct bpf_mem_alloc *ma)
{
	struct bpf_mem_caches *cc;
	struct bpf_mem_cache *c;
	int cpu, i, rcu_in_progress;

	if (ma->cache) {
		rcu_in_progress = 0;
		for_each_possible_cpu(cpu) {
			c = per_cpu_ptr(ma->cache, cpu);
			WRITE_ONCE(c->draining, true);
			irq_work_sync(&c->refill_work);
			drain_mem_cache(c);
			rcu_in_progress += atomic_read(&c->call_rcu_ttrace_in_progress);
			rcu_in_progress += atomic_read(&c->call_rcu_in_progress);
		}
		 
		if (c->objcg)
			obj_cgroup_put(c->objcg);
		destroy_mem_alloc(ma, rcu_in_progress);
	}
	if (ma->caches) {
		rcu_in_progress = 0;
		for_each_possible_cpu(cpu) {
			cc = per_cpu_ptr(ma->caches, cpu);
			for (i = 0; i < NUM_CACHES; i++) {
				c = &cc->cache[i];
				WRITE_ONCE(c->draining, true);
				irq_work_sync(&c->refill_work);
				drain_mem_cache(c);
				rcu_in_progress += atomic_read(&c->call_rcu_ttrace_in_progress);
				rcu_in_progress += atomic_read(&c->call_rcu_in_progress);
			}
		}
		if (c->objcg)
			obj_cgroup_put(c->objcg);
		destroy_mem_alloc(ma, rcu_in_progress);
	}
}

 
static void notrace *unit_alloc(struct bpf_mem_cache *c)
{
	struct llist_node *llnode = NULL;
	unsigned long flags;
	int cnt = 0;

	 
	local_irq_save(flags);
	if (local_inc_return(&c->active) == 1) {
		llnode = __llist_del_first(&c->free_llist);
		if (llnode) {
			cnt = --c->free_cnt;
			*(struct bpf_mem_cache **)llnode = c;
		}
	}
	local_dec(&c->active);
	local_irq_restore(flags);

	WARN_ON(cnt < 0);

	if (cnt < c->low_watermark)
		irq_work_raise(c);
	return llnode;
}

 
static void notrace unit_free(struct bpf_mem_cache *c, void *ptr)
{
	struct llist_node *llnode = ptr - LLIST_NODE_SZ;
	unsigned long flags;
	int cnt = 0;

	BUILD_BUG_ON(LLIST_NODE_SZ > 8);

	 
	c->tgt = *(struct bpf_mem_cache **)llnode;

	local_irq_save(flags);
	if (local_inc_return(&c->active) == 1) {
		__llist_add(llnode, &c->free_llist);
		cnt = ++c->free_cnt;
	} else {
		 
		llist_add(llnode, &c->free_llist_extra);
	}
	local_dec(&c->active);
	local_irq_restore(flags);

	if (cnt > c->high_watermark)
		 
		irq_work_raise(c);
}

static void notrace unit_free_rcu(struct bpf_mem_cache *c, void *ptr)
{
	struct llist_node *llnode = ptr - LLIST_NODE_SZ;
	unsigned long flags;

	c->tgt = *(struct bpf_mem_cache **)llnode;

	local_irq_save(flags);
	if (local_inc_return(&c->active) == 1) {
		if (__llist_add(llnode, &c->free_by_rcu))
			c->free_by_rcu_tail = llnode;
	} else {
		llist_add(llnode, &c->free_llist_extra_rcu);
	}
	local_dec(&c->active);
	local_irq_restore(flags);

	if (!atomic_read(&c->call_rcu_in_progress))
		irq_work_raise(c);
}

 
void notrace *bpf_mem_alloc(struct bpf_mem_alloc *ma, size_t size)
{
	int idx;
	void *ret;

	if (!size)
		return NULL;

	idx = bpf_mem_cache_idx(size + LLIST_NODE_SZ);
	if (idx < 0)
		return NULL;

	ret = unit_alloc(this_cpu_ptr(ma->caches)->cache + idx);
	return !ret ? NULL : ret + LLIST_NODE_SZ;
}

void notrace bpf_mem_free(struct bpf_mem_alloc *ma, void *ptr)
{
	struct bpf_mem_cache *c;
	int idx;

	if (!ptr)
		return;

	c = *(void **)(ptr - LLIST_NODE_SZ);
	idx = bpf_mem_cache_idx(c->unit_size);
	if (WARN_ON_ONCE(idx < 0))
		return;

	unit_free(this_cpu_ptr(ma->caches)->cache + idx, ptr);
}

void notrace bpf_mem_free_rcu(struct bpf_mem_alloc *ma, void *ptr)
{
	struct bpf_mem_cache *c;
	int idx;

	if (!ptr)
		return;

	c = *(void **)(ptr - LLIST_NODE_SZ);
	idx = bpf_mem_cache_idx(c->unit_size);
	if (WARN_ON_ONCE(idx < 0))
		return;

	unit_free_rcu(this_cpu_ptr(ma->caches)->cache + idx, ptr);
}

void notrace *bpf_mem_cache_alloc(struct bpf_mem_alloc *ma)
{
	void *ret;

	ret = unit_alloc(this_cpu_ptr(ma->cache));
	return !ret ? NULL : ret + LLIST_NODE_SZ;
}

void notrace bpf_mem_cache_free(struct bpf_mem_alloc *ma, void *ptr)
{
	if (!ptr)
		return;

	unit_free(this_cpu_ptr(ma->cache), ptr);
}

void notrace bpf_mem_cache_free_rcu(struct bpf_mem_alloc *ma, void *ptr)
{
	if (!ptr)
		return;

	unit_free_rcu(this_cpu_ptr(ma->cache), ptr);
}

 
void bpf_mem_cache_raw_free(void *ptr)
{
	if (!ptr)
		return;

	kfree(ptr - LLIST_NODE_SZ);
}

 
void notrace *bpf_mem_cache_alloc_flags(struct bpf_mem_alloc *ma, gfp_t flags)
{
	struct bpf_mem_cache *c;
	void *ret;

	c = this_cpu_ptr(ma->cache);

	ret = unit_alloc(c);
	if (!ret && flags == GFP_KERNEL) {
		struct mem_cgroup *memcg, *old_memcg;

		memcg = get_memcg(c);
		old_memcg = set_active_memcg(memcg);
		ret = __alloc(c, NUMA_NO_NODE, GFP_KERNEL | __GFP_NOWARN | __GFP_ACCOUNT);
		if (ret)
			*(struct bpf_mem_cache **)ret = c;
		set_active_memcg(old_memcg);
		mem_cgroup_put(memcg);
	}

	return !ret ? NULL : ret + LLIST_NODE_SZ;
}
