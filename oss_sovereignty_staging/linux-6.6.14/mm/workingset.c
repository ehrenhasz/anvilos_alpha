
 

#include <linux/memcontrol.h>
#include <linux/mm_inline.h>
#include <linux/writeback.h>
#include <linux/shmem_fs.h>
#include <linux/pagemap.h>
#include <linux/atomic.h>
#include <linux/module.h>
#include <linux/swap.h>
#include <linux/dax.h>
#include <linux/fs.h>
#include <linux/mm.h>

 

#define WORKINGSET_SHIFT 1
#define EVICTION_SHIFT	((BITS_PER_LONG - BITS_PER_XA_VALUE) +	\
			 WORKINGSET_SHIFT + NODES_SHIFT + \
			 MEM_CGROUP_ID_SHIFT)
#define EVICTION_MASK	(~0UL >> EVICTION_SHIFT)

 
static unsigned int bucket_order __read_mostly;

static void *pack_shadow(int memcgid, pg_data_t *pgdat, unsigned long eviction,
			 bool workingset)
{
	eviction &= EVICTION_MASK;
	eviction = (eviction << MEM_CGROUP_ID_SHIFT) | memcgid;
	eviction = (eviction << NODES_SHIFT) | pgdat->node_id;
	eviction = (eviction << WORKINGSET_SHIFT) | workingset;

	return xa_mk_value(eviction);
}

static void unpack_shadow(void *shadow, int *memcgidp, pg_data_t **pgdat,
			  unsigned long *evictionp, bool *workingsetp)
{
	unsigned long entry = xa_to_value(shadow);
	int memcgid, nid;
	bool workingset;

	workingset = entry & ((1UL << WORKINGSET_SHIFT) - 1);
	entry >>= WORKINGSET_SHIFT;
	nid = entry & ((1UL << NODES_SHIFT) - 1);
	entry >>= NODES_SHIFT;
	memcgid = entry & ((1UL << MEM_CGROUP_ID_SHIFT) - 1);
	entry >>= MEM_CGROUP_ID_SHIFT;

	*memcgidp = memcgid;
	*pgdat = NODE_DATA(nid);
	*evictionp = entry;
	*workingsetp = workingset;
}

#ifdef CONFIG_LRU_GEN

static void *lru_gen_eviction(struct folio *folio)
{
	int hist;
	unsigned long token;
	unsigned long min_seq;
	struct lruvec *lruvec;
	struct lru_gen_folio *lrugen;
	int type = folio_is_file_lru(folio);
	int delta = folio_nr_pages(folio);
	int refs = folio_lru_refs(folio);
	int tier = lru_tier_from_refs(refs);
	struct mem_cgroup *memcg = folio_memcg(folio);
	struct pglist_data *pgdat = folio_pgdat(folio);

	BUILD_BUG_ON(LRU_GEN_WIDTH + LRU_REFS_WIDTH > BITS_PER_LONG - EVICTION_SHIFT);

	lruvec = mem_cgroup_lruvec(memcg, pgdat);
	lrugen = &lruvec->lrugen;
	min_seq = READ_ONCE(lrugen->min_seq[type]);
	token = (min_seq << LRU_REFS_WIDTH) | max(refs - 1, 0);

	hist = lru_hist_from_seq(min_seq);
	atomic_long_add(delta, &lrugen->evicted[hist][type][tier]);

	return pack_shadow(mem_cgroup_id(memcg), pgdat, token, refs);
}

 
static bool lru_gen_test_recent(void *shadow, bool file, struct lruvec **lruvec,
				unsigned long *token, bool *workingset)
{
	int memcg_id;
	unsigned long min_seq;
	struct mem_cgroup *memcg;
	struct pglist_data *pgdat;

	unpack_shadow(shadow, &memcg_id, &pgdat, token, workingset);

	memcg = mem_cgroup_from_id(memcg_id);
	*lruvec = mem_cgroup_lruvec(memcg, pgdat);

	min_seq = READ_ONCE((*lruvec)->lrugen.min_seq[file]);
	return (*token >> LRU_REFS_WIDTH) == (min_seq & (EVICTION_MASK >> LRU_REFS_WIDTH));
}

static void lru_gen_refault(struct folio *folio, void *shadow)
{
	bool recent;
	int hist, tier, refs;
	bool workingset;
	unsigned long token;
	struct lruvec *lruvec;
	struct lru_gen_folio *lrugen;
	int type = folio_is_file_lru(folio);
	int delta = folio_nr_pages(folio);

	rcu_read_lock();

	recent = lru_gen_test_recent(shadow, type, &lruvec, &token, &workingset);
	if (lruvec != folio_lruvec(folio))
		goto unlock;

	mod_lruvec_state(lruvec, WORKINGSET_REFAULT_BASE + type, delta);

	if (!recent)
		goto unlock;

	lrugen = &lruvec->lrugen;

	hist = lru_hist_from_seq(READ_ONCE(lrugen->min_seq[type]));
	 
	refs = (token & (BIT(LRU_REFS_WIDTH) - 1)) + workingset;
	tier = lru_tier_from_refs(refs);

	atomic_long_add(delta, &lrugen->refaulted[hist][type][tier]);
	mod_lruvec_state(lruvec, WORKINGSET_ACTIVATE_BASE + type, delta);

	 
	if (lru_gen_in_fault() || refs >= BIT(LRU_REFS_WIDTH) - 1) {
		set_mask_bits(&folio->flags, 0, LRU_REFS_MASK | BIT(PG_workingset));
		mod_lruvec_state(lruvec, WORKINGSET_RESTORE_BASE + type, delta);
	}
unlock:
	rcu_read_unlock();
}

#else  

static void *lru_gen_eviction(struct folio *folio)
{
	return NULL;
}

static bool lru_gen_test_recent(void *shadow, bool file, struct lruvec **lruvec,
				unsigned long *token, bool *workingset)
{
	return false;
}

static void lru_gen_refault(struct folio *folio, void *shadow)
{
}

#endif  

 
void workingset_age_nonresident(struct lruvec *lruvec, unsigned long nr_pages)
{
	 
	do {
		atomic_long_add(nr_pages, &lruvec->nonresident_age);
	} while ((lruvec = parent_lruvec(lruvec)));
}

 
void *workingset_eviction(struct folio *folio, struct mem_cgroup *target_memcg)
{
	struct pglist_data *pgdat = folio_pgdat(folio);
	unsigned long eviction;
	struct lruvec *lruvec;
	int memcgid;

	 
	VM_BUG_ON_FOLIO(folio_test_lru(folio), folio);
	VM_BUG_ON_FOLIO(folio_ref_count(folio), folio);
	VM_BUG_ON_FOLIO(!folio_test_locked(folio), folio);

	if (lru_gen_enabled())
		return lru_gen_eviction(folio);

	lruvec = mem_cgroup_lruvec(target_memcg, pgdat);
	 
	memcgid = mem_cgroup_id(lruvec_memcg(lruvec));
	eviction = atomic_long_read(&lruvec->nonresident_age);
	eviction >>= bucket_order;
	workingset_age_nonresident(lruvec, folio_nr_pages(folio));
	return pack_shadow(memcgid, pgdat, eviction,
				folio_test_workingset(folio));
}

 
bool workingset_test_recent(void *shadow, bool file, bool *workingset)
{
	struct mem_cgroup *eviction_memcg;
	struct lruvec *eviction_lruvec;
	unsigned long refault_distance;
	unsigned long workingset_size;
	unsigned long refault;
	int memcgid;
	struct pglist_data *pgdat;
	unsigned long eviction;

	if (lru_gen_enabled())
		return lru_gen_test_recent(shadow, file, &eviction_lruvec, &eviction, workingset);

	unpack_shadow(shadow, &memcgid, &pgdat, &eviction, workingset);
	eviction <<= bucket_order;

	 
	eviction_memcg = mem_cgroup_from_id(memcgid);
	if (!mem_cgroup_disabled() && !eviction_memcg)
		return false;

	eviction_lruvec = mem_cgroup_lruvec(eviction_memcg, pgdat);
	refault = atomic_long_read(&eviction_lruvec->nonresident_age);

	 
	refault_distance = (refault - eviction) & EVICTION_MASK;

	 
	workingset_size = lruvec_page_state(eviction_lruvec, NR_ACTIVE_FILE);
	if (!file) {
		workingset_size += lruvec_page_state(eviction_lruvec,
						     NR_INACTIVE_FILE);
	}
	if (mem_cgroup_get_nr_swap_pages(eviction_memcg) > 0) {
		workingset_size += lruvec_page_state(eviction_lruvec,
						     NR_ACTIVE_ANON);
		if (file) {
			workingset_size += lruvec_page_state(eviction_lruvec,
						     NR_INACTIVE_ANON);
		}
	}

	return refault_distance <= workingset_size;
}

 
void workingset_refault(struct folio *folio, void *shadow)
{
	bool file = folio_is_file_lru(folio);
	struct pglist_data *pgdat;
	struct mem_cgroup *memcg;
	struct lruvec *lruvec;
	bool workingset;
	long nr;

	if (lru_gen_enabled()) {
		lru_gen_refault(folio, shadow);
		return;
	}

	 
	mem_cgroup_flush_stats_ratelimited();

	rcu_read_lock();

	 
	nr = folio_nr_pages(folio);
	memcg = folio_memcg(folio);
	pgdat = folio_pgdat(folio);
	lruvec = mem_cgroup_lruvec(memcg, pgdat);

	mod_lruvec_state(lruvec, WORKINGSET_REFAULT_BASE + file, nr);

	if (!workingset_test_recent(shadow, file, &workingset))
		goto out;

	folio_set_active(folio);
	workingset_age_nonresident(lruvec, nr);
	mod_lruvec_state(lruvec, WORKINGSET_ACTIVATE_BASE + file, nr);

	 
	if (workingset) {
		folio_set_workingset(folio);
		 
		lru_note_cost_refault(folio);
		mod_lruvec_state(lruvec, WORKINGSET_RESTORE_BASE + file, nr);
	}
out:
	rcu_read_unlock();
}

 
void workingset_activation(struct folio *folio)
{
	struct mem_cgroup *memcg;

	rcu_read_lock();
	 
	memcg = folio_memcg_rcu(folio);
	if (!mem_cgroup_disabled() && !memcg)
		goto out;
	workingset_age_nonresident(folio_lruvec(folio), folio_nr_pages(folio));
out:
	rcu_read_unlock();
}

 

struct list_lru shadow_nodes;

void workingset_update_node(struct xa_node *node)
{
	struct address_space *mapping;

	 
	mapping = container_of(node->array, struct address_space, i_pages);
	lockdep_assert_held(&mapping->i_pages.xa_lock);

	if (node->count && node->count == node->nr_values) {
		if (list_empty(&node->private_list)) {
			list_lru_add(&shadow_nodes, &node->private_list);
			__inc_lruvec_kmem_state(node, WORKINGSET_NODES);
		}
	} else {
		if (!list_empty(&node->private_list)) {
			list_lru_del(&shadow_nodes, &node->private_list);
			__dec_lruvec_kmem_state(node, WORKINGSET_NODES);
		}
	}
}

static unsigned long count_shadow_nodes(struct shrinker *shrinker,
					struct shrink_control *sc)
{
	unsigned long max_nodes;
	unsigned long nodes;
	unsigned long pages;

	nodes = list_lru_shrink_count(&shadow_nodes, sc);
	if (!nodes)
		return SHRINK_EMPTY;

	 
#ifdef CONFIG_MEMCG
	if (sc->memcg) {
		struct lruvec *lruvec;
		int i;

		mem_cgroup_flush_stats();
		lruvec = mem_cgroup_lruvec(sc->memcg, NODE_DATA(sc->nid));
		for (pages = 0, i = 0; i < NR_LRU_LISTS; i++)
			pages += lruvec_page_state_local(lruvec,
							 NR_LRU_BASE + i);
		pages += lruvec_page_state_local(
			lruvec, NR_SLAB_RECLAIMABLE_B) >> PAGE_SHIFT;
		pages += lruvec_page_state_local(
			lruvec, NR_SLAB_UNRECLAIMABLE_B) >> PAGE_SHIFT;
	} else
#endif
		pages = node_present_pages(sc->nid);

	max_nodes = pages >> (XA_CHUNK_SHIFT - 3);

	if (nodes <= max_nodes)
		return 0;
	return nodes - max_nodes;
}

static enum lru_status shadow_lru_isolate(struct list_head *item,
					  struct list_lru_one *lru,
					  spinlock_t *lru_lock,
					  void *arg) __must_hold(lru_lock)
{
	struct xa_node *node = container_of(item, struct xa_node, private_list);
	struct address_space *mapping;
	int ret;

	 

	mapping = container_of(node->array, struct address_space, i_pages);

	 
	if (!xa_trylock(&mapping->i_pages)) {
		spin_unlock_irq(lru_lock);
		ret = LRU_RETRY;
		goto out;
	}

	 
	if (mapping->host != NULL) {
		if (!spin_trylock(&mapping->host->i_lock)) {
			xa_unlock(&mapping->i_pages);
			spin_unlock_irq(lru_lock);
			ret = LRU_RETRY;
			goto out;
		}
	}

	list_lru_isolate(lru, item);
	__dec_lruvec_kmem_state(node, WORKINGSET_NODES);

	spin_unlock(lru_lock);

	 
	if (WARN_ON_ONCE(!node->nr_values))
		goto out_invalid;
	if (WARN_ON_ONCE(node->count != node->nr_values))
		goto out_invalid;
	xa_delete_node(node, workingset_update_node);
	__inc_lruvec_kmem_state(node, WORKINGSET_NODERECLAIM);

out_invalid:
	xa_unlock_irq(&mapping->i_pages);
	if (mapping->host != NULL) {
		if (mapping_shrinkable(mapping))
			inode_add_lru(mapping->host);
		spin_unlock(&mapping->host->i_lock);
	}
	ret = LRU_REMOVED_RETRY;
out:
	cond_resched();
	spin_lock_irq(lru_lock);
	return ret;
}

static unsigned long scan_shadow_nodes(struct shrinker *shrinker,
				       struct shrink_control *sc)
{
	 
	return list_lru_shrink_walk_irq(&shadow_nodes, sc, shadow_lru_isolate,
					NULL);
}

static struct shrinker workingset_shadow_shrinker = {
	.count_objects = count_shadow_nodes,
	.scan_objects = scan_shadow_nodes,
	.seeks = 0,  
	.flags = SHRINKER_NUMA_AWARE | SHRINKER_MEMCG_AWARE,
};

 
static struct lock_class_key shadow_nodes_key;

static int __init workingset_init(void)
{
	unsigned int timestamp_bits;
	unsigned int max_order;
	int ret;

	BUILD_BUG_ON(BITS_PER_LONG < EVICTION_SHIFT);
	 
	timestamp_bits = BITS_PER_LONG - EVICTION_SHIFT;
	max_order = fls_long(totalram_pages() - 1);
	if (max_order > timestamp_bits)
		bucket_order = max_order - timestamp_bits;
	pr_info("workingset: timestamp_bits=%d max_order=%d bucket_order=%u\n",
	       timestamp_bits, max_order, bucket_order);

	ret = prealloc_shrinker(&workingset_shadow_shrinker, "mm-shadow");
	if (ret)
		goto err;
	ret = __list_lru_init(&shadow_nodes, true, &shadow_nodes_key,
			      &workingset_shadow_shrinker);
	if (ret)
		goto err_list_lru;
	register_shrinker_prepared(&workingset_shadow_shrinker);
	return 0;
err_list_lru:
	free_prealloced_shrinker(&workingset_shadow_shrinker);
err:
	return ret;
}
module_init(workingset_init);
