
 

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/module.h>
#include <linux/cpu.h>
#include <linux/highmem.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/types.h>
#include <linux/atomic.h>
#include <linux/rbtree.h>
#include <linux/swap.h>
#include <linux/crypto.h>
#include <linux/scatterlist.h>
#include <linux/mempool.h>
#include <linux/zpool.h>
#include <crypto/acompress.h>
#include <linux/zswap.h>
#include <linux/mm_types.h>
#include <linux/page-flags.h>
#include <linux/swapops.h>
#include <linux/writeback.h>
#include <linux/pagemap.h>
#include <linux/workqueue.h>

#include "swap.h"
#include "internal.h"

 
 
u64 zswap_pool_total_size;
 
atomic_t zswap_stored_pages = ATOMIC_INIT(0);
 
static atomic_t zswap_same_filled_pages = ATOMIC_INIT(0);

 

 
static u64 zswap_pool_limit_hit;
 
static u64 zswap_written_back_pages;
 
static u64 zswap_reject_reclaim_fail;
 
static u64 zswap_reject_compress_poor;
 
static u64 zswap_reject_alloc_fail;
 
static u64 zswap_reject_kmemcache_fail;
 
static u64 zswap_duplicate_entry;

 
static struct workqueue_struct *shrink_wq;
 
static bool zswap_pool_reached_full;

 

#define ZSWAP_PARAM_UNSET ""

static int zswap_setup(void);

 
static bool zswap_enabled = IS_ENABLED(CONFIG_ZSWAP_DEFAULT_ON);
static int zswap_enabled_param_set(const char *,
				   const struct kernel_param *);
static const struct kernel_param_ops zswap_enabled_param_ops = {
	.set =		zswap_enabled_param_set,
	.get =		param_get_bool,
};
module_param_cb(enabled, &zswap_enabled_param_ops, &zswap_enabled, 0644);

 
static char *zswap_compressor = CONFIG_ZSWAP_COMPRESSOR_DEFAULT;
static int zswap_compressor_param_set(const char *,
				      const struct kernel_param *);
static const struct kernel_param_ops zswap_compressor_param_ops = {
	.set =		zswap_compressor_param_set,
	.get =		param_get_charp,
	.free =		param_free_charp,
};
module_param_cb(compressor, &zswap_compressor_param_ops,
		&zswap_compressor, 0644);

 
static char *zswap_zpool_type = CONFIG_ZSWAP_ZPOOL_DEFAULT;
static int zswap_zpool_param_set(const char *, const struct kernel_param *);
static const struct kernel_param_ops zswap_zpool_param_ops = {
	.set =		zswap_zpool_param_set,
	.get =		param_get_charp,
	.free =		param_free_charp,
};
module_param_cb(zpool, &zswap_zpool_param_ops, &zswap_zpool_type, 0644);

 
static unsigned int zswap_max_pool_percent = 20;
module_param_named(max_pool_percent, zswap_max_pool_percent, uint, 0644);

 
static unsigned int zswap_accept_thr_percent = 90;  
module_param_named(accept_threshold_percent, zswap_accept_thr_percent,
		   uint, 0644);

 
static bool zswap_same_filled_pages_enabled = true;
module_param_named(same_filled_pages_enabled, zswap_same_filled_pages_enabled,
		   bool, 0644);

 
static bool zswap_non_same_filled_pages_enabled = true;
module_param_named(non_same_filled_pages_enabled, zswap_non_same_filled_pages_enabled,
		   bool, 0644);

static bool zswap_exclusive_loads_enabled = IS_ENABLED(
		CONFIG_ZSWAP_EXCLUSIVE_LOADS_DEFAULT_ON);
module_param_named(exclusive_loads, zswap_exclusive_loads_enabled, bool, 0644);

 
#define ZSWAP_NR_ZPOOLS 32

 

struct crypto_acomp_ctx {
	struct crypto_acomp *acomp;
	struct acomp_req *req;
	struct crypto_wait wait;
	u8 *dstmem;
	struct mutex *mutex;
};

 
struct zswap_pool {
	struct zpool *zpools[ZSWAP_NR_ZPOOLS];
	struct crypto_acomp_ctx __percpu *acomp_ctx;
	struct kref kref;
	struct list_head list;
	struct work_struct release_work;
	struct work_struct shrink_work;
	struct hlist_node node;
	char tfm_name[CRYPTO_MAX_ALG_NAME];
	struct list_head lru;
	spinlock_t lru_lock;
};

 
struct zswap_entry {
	struct rb_node rbnode;
	swp_entry_t swpentry;
	int refcount;
	unsigned int length;
	struct zswap_pool *pool;
	union {
		unsigned long handle;
		unsigned long value;
	};
	struct obj_cgroup *objcg;
	struct list_head lru;
};

 
struct zswap_tree {
	struct rb_root rbroot;
	spinlock_t lock;
};

static struct zswap_tree *zswap_trees[MAX_SWAPFILES];

 
static LIST_HEAD(zswap_pools);
 
static DEFINE_SPINLOCK(zswap_pools_lock);
 
static atomic_t zswap_pools_count = ATOMIC_INIT(0);

enum zswap_init_type {
	ZSWAP_UNINIT,
	ZSWAP_INIT_SUCCEED,
	ZSWAP_INIT_FAILED
};

static enum zswap_init_type zswap_init_state;

 
static DEFINE_MUTEX(zswap_init_lock);

 
static bool zswap_has_pool;

 

#define zswap_pool_debug(msg, p)				\
	pr_debug("%s pool %s/%s\n", msg, (p)->tfm_name,		\
		 zpool_get_type((p)->zpools[0]))

static int zswap_writeback_entry(struct zswap_entry *entry,
				 struct zswap_tree *tree);
static int zswap_pool_get(struct zswap_pool *pool);
static void zswap_pool_put(struct zswap_pool *pool);

static bool zswap_is_full(void)
{
	return totalram_pages() * zswap_max_pool_percent / 100 <
			DIV_ROUND_UP(zswap_pool_total_size, PAGE_SIZE);
}

static bool zswap_can_accept(void)
{
	return totalram_pages() * zswap_accept_thr_percent / 100 *
				zswap_max_pool_percent / 100 >
			DIV_ROUND_UP(zswap_pool_total_size, PAGE_SIZE);
}

static void zswap_update_total_size(void)
{
	struct zswap_pool *pool;
	u64 total = 0;
	int i;

	rcu_read_lock();

	list_for_each_entry_rcu(pool, &zswap_pools, list)
		for (i = 0; i < ZSWAP_NR_ZPOOLS; i++)
			total += zpool_get_total_size(pool->zpools[i]);

	rcu_read_unlock();

	zswap_pool_total_size = total;
}

 
static struct kmem_cache *zswap_entry_cache;

static struct zswap_entry *zswap_entry_cache_alloc(gfp_t gfp)
{
	struct zswap_entry *entry;
	entry = kmem_cache_alloc(zswap_entry_cache, gfp);
	if (!entry)
		return NULL;
	entry->refcount = 1;
	RB_CLEAR_NODE(&entry->rbnode);
	return entry;
}

static void zswap_entry_cache_free(struct zswap_entry *entry)
{
	kmem_cache_free(zswap_entry_cache, entry);
}

 
static struct zswap_entry *zswap_rb_search(struct rb_root *root, pgoff_t offset)
{
	struct rb_node *node = root->rb_node;
	struct zswap_entry *entry;
	pgoff_t entry_offset;

	while (node) {
		entry = rb_entry(node, struct zswap_entry, rbnode);
		entry_offset = swp_offset(entry->swpentry);
		if (entry_offset > offset)
			node = node->rb_left;
		else if (entry_offset < offset)
			node = node->rb_right;
		else
			return entry;
	}
	return NULL;
}

 
static int zswap_rb_insert(struct rb_root *root, struct zswap_entry *entry,
			struct zswap_entry **dupentry)
{
	struct rb_node **link = &root->rb_node, *parent = NULL;
	struct zswap_entry *myentry;
	pgoff_t myentry_offset, entry_offset = swp_offset(entry->swpentry);

	while (*link) {
		parent = *link;
		myentry = rb_entry(parent, struct zswap_entry, rbnode);
		myentry_offset = swp_offset(myentry->swpentry);
		if (myentry_offset > entry_offset)
			link = &(*link)->rb_left;
		else if (myentry_offset < entry_offset)
			link = &(*link)->rb_right;
		else {
			*dupentry = myentry;
			return -EEXIST;
		}
	}
	rb_link_node(&entry->rbnode, parent, link);
	rb_insert_color(&entry->rbnode, root);
	return 0;
}

static bool zswap_rb_erase(struct rb_root *root, struct zswap_entry *entry)
{
	if (!RB_EMPTY_NODE(&entry->rbnode)) {
		rb_erase(&entry->rbnode, root);
		RB_CLEAR_NODE(&entry->rbnode);
		return true;
	}
	return false;
}

static struct zpool *zswap_find_zpool(struct zswap_entry *entry)
{
	int i = 0;

	if (ZSWAP_NR_ZPOOLS > 1)
		i = hash_ptr(entry, ilog2(ZSWAP_NR_ZPOOLS));

	return entry->pool->zpools[i];
}

 
static void zswap_free_entry(struct zswap_entry *entry)
{
	if (entry->objcg) {
		obj_cgroup_uncharge_zswap(entry->objcg, entry->length);
		obj_cgroup_put(entry->objcg);
	}
	if (!entry->length)
		atomic_dec(&zswap_same_filled_pages);
	else {
		spin_lock(&entry->pool->lru_lock);
		list_del(&entry->lru);
		spin_unlock(&entry->pool->lru_lock);
		zpool_free(zswap_find_zpool(entry), entry->handle);
		zswap_pool_put(entry->pool);
	}
	zswap_entry_cache_free(entry);
	atomic_dec(&zswap_stored_pages);
	zswap_update_total_size();
}

 
static void zswap_entry_get(struct zswap_entry *entry)
{
	entry->refcount++;
}

 
static void zswap_entry_put(struct zswap_tree *tree,
			struct zswap_entry *entry)
{
	int refcount = --entry->refcount;

	WARN_ON_ONCE(refcount < 0);
	if (refcount == 0) {
		WARN_ON_ONCE(!RB_EMPTY_NODE(&entry->rbnode));
		zswap_free_entry(entry);
	}
}

 
static struct zswap_entry *zswap_entry_find_get(struct rb_root *root,
				pgoff_t offset)
{
	struct zswap_entry *entry;

	entry = zswap_rb_search(root, offset);
	if (entry)
		zswap_entry_get(entry);

	return entry;
}

 
static DEFINE_PER_CPU(u8 *, zswap_dstmem);
 
static DEFINE_PER_CPU(struct mutex *, zswap_mutex);

static int zswap_dstmem_prepare(unsigned int cpu)
{
	struct mutex *mutex;
	u8 *dst;

	dst = kmalloc_node(PAGE_SIZE * 2, GFP_KERNEL, cpu_to_node(cpu));
	if (!dst)
		return -ENOMEM;

	mutex = kmalloc_node(sizeof(*mutex), GFP_KERNEL, cpu_to_node(cpu));
	if (!mutex) {
		kfree(dst);
		return -ENOMEM;
	}

	mutex_init(mutex);
	per_cpu(zswap_dstmem, cpu) = dst;
	per_cpu(zswap_mutex, cpu) = mutex;
	return 0;
}

static int zswap_dstmem_dead(unsigned int cpu)
{
	struct mutex *mutex;
	u8 *dst;

	mutex = per_cpu(zswap_mutex, cpu);
	kfree(mutex);
	per_cpu(zswap_mutex, cpu) = NULL;

	dst = per_cpu(zswap_dstmem, cpu);
	kfree(dst);
	per_cpu(zswap_dstmem, cpu) = NULL;

	return 0;
}

static int zswap_cpu_comp_prepare(unsigned int cpu, struct hlist_node *node)
{
	struct zswap_pool *pool = hlist_entry(node, struct zswap_pool, node);
	struct crypto_acomp_ctx *acomp_ctx = per_cpu_ptr(pool->acomp_ctx, cpu);
	struct crypto_acomp *acomp;
	struct acomp_req *req;

	acomp = crypto_alloc_acomp_node(pool->tfm_name, 0, 0, cpu_to_node(cpu));
	if (IS_ERR(acomp)) {
		pr_err("could not alloc crypto acomp %s : %ld\n",
				pool->tfm_name, PTR_ERR(acomp));
		return PTR_ERR(acomp);
	}
	acomp_ctx->acomp = acomp;

	req = acomp_request_alloc(acomp_ctx->acomp);
	if (!req) {
		pr_err("could not alloc crypto acomp_request %s\n",
		       pool->tfm_name);
		crypto_free_acomp(acomp_ctx->acomp);
		return -ENOMEM;
	}
	acomp_ctx->req = req;

	crypto_init_wait(&acomp_ctx->wait);
	 
	acomp_request_set_callback(req, CRYPTO_TFM_REQ_MAY_BACKLOG,
				   crypto_req_done, &acomp_ctx->wait);

	acomp_ctx->mutex = per_cpu(zswap_mutex, cpu);
	acomp_ctx->dstmem = per_cpu(zswap_dstmem, cpu);

	return 0;
}

static int zswap_cpu_comp_dead(unsigned int cpu, struct hlist_node *node)
{
	struct zswap_pool *pool = hlist_entry(node, struct zswap_pool, node);
	struct crypto_acomp_ctx *acomp_ctx = per_cpu_ptr(pool->acomp_ctx, cpu);

	if (!IS_ERR_OR_NULL(acomp_ctx)) {
		if (!IS_ERR_OR_NULL(acomp_ctx->req))
			acomp_request_free(acomp_ctx->req);
		if (!IS_ERR_OR_NULL(acomp_ctx->acomp))
			crypto_free_acomp(acomp_ctx->acomp);
	}

	return 0;
}

 

static struct zswap_pool *__zswap_pool_current(void)
{
	struct zswap_pool *pool;

	pool = list_first_or_null_rcu(&zswap_pools, typeof(*pool), list);
	WARN_ONCE(!pool && zswap_has_pool,
		  "%s: no page storage pool!\n", __func__);

	return pool;
}

static struct zswap_pool *zswap_pool_current(void)
{
	assert_spin_locked(&zswap_pools_lock);

	return __zswap_pool_current();
}

static struct zswap_pool *zswap_pool_current_get(void)
{
	struct zswap_pool *pool;

	rcu_read_lock();

	pool = __zswap_pool_current();
	if (!zswap_pool_get(pool))
		pool = NULL;

	rcu_read_unlock();

	return pool;
}

static struct zswap_pool *zswap_pool_last_get(void)
{
	struct zswap_pool *pool, *last = NULL;

	rcu_read_lock();

	list_for_each_entry_rcu(pool, &zswap_pools, list)
		last = pool;
	WARN_ONCE(!last && zswap_has_pool,
		  "%s: no page storage pool!\n", __func__);
	if (!zswap_pool_get(last))
		last = NULL;

	rcu_read_unlock();

	return last;
}

 
static struct zswap_pool *zswap_pool_find_get(char *type, char *compressor)
{
	struct zswap_pool *pool;

	assert_spin_locked(&zswap_pools_lock);

	list_for_each_entry_rcu(pool, &zswap_pools, list) {
		if (strcmp(pool->tfm_name, compressor))
			continue;
		 
		if (strcmp(zpool_get_type(pool->zpools[0]), type))
			continue;
		 
		if (!zswap_pool_get(pool))
			continue;
		return pool;
	}

	return NULL;
}

 
static void zswap_invalidate_entry(struct zswap_tree *tree,
				   struct zswap_entry *entry)
{
	if (zswap_rb_erase(&tree->rbroot, entry))
		zswap_entry_put(tree, entry);
}

static int zswap_reclaim_entry(struct zswap_pool *pool)
{
	struct zswap_entry *entry;
	struct zswap_tree *tree;
	pgoff_t swpoffset;
	int ret;

	 
	spin_lock(&pool->lru_lock);
	if (list_empty(&pool->lru)) {
		spin_unlock(&pool->lru_lock);
		return -EINVAL;
	}
	entry = list_last_entry(&pool->lru, struct zswap_entry, lru);
	list_del_init(&entry->lru);
	 
	swpoffset = swp_offset(entry->swpentry);
	tree = zswap_trees[swp_type(entry->swpentry)];
	spin_unlock(&pool->lru_lock);

	 
	spin_lock(&tree->lock);
	if (entry != zswap_rb_search(&tree->rbroot, swpoffset)) {
		ret = -EAGAIN;
		goto unlock;
	}
	 
	zswap_entry_get(entry);
	spin_unlock(&tree->lock);

	ret = zswap_writeback_entry(entry, tree);

	spin_lock(&tree->lock);
	if (ret) {
		 
		spin_lock(&pool->lru_lock);
		list_move(&entry->lru, &pool->lru);
		spin_unlock(&pool->lru_lock);
		goto put_unlock;
	}

	 
	zswap_invalidate_entry(tree, entry);

put_unlock:
	 
	zswap_entry_put(tree, entry);
unlock:
	spin_unlock(&tree->lock);
	return ret ? -EAGAIN : 0;
}

static void shrink_worker(struct work_struct *w)
{
	struct zswap_pool *pool = container_of(w, typeof(*pool),
						shrink_work);
	int ret, failures = 0;

	do {
		ret = zswap_reclaim_entry(pool);
		if (ret) {
			zswap_reject_reclaim_fail++;
			if (ret != -EAGAIN)
				break;
			if (++failures == MAX_RECLAIM_RETRIES)
				break;
		}
		cond_resched();
	} while (!zswap_can_accept());
	zswap_pool_put(pool);
}

static struct zswap_pool *zswap_pool_create(char *type, char *compressor)
{
	int i;
	struct zswap_pool *pool;
	char name[38];  
	gfp_t gfp = __GFP_NORETRY | __GFP_NOWARN | __GFP_KSWAPD_RECLAIM;
	int ret;

	if (!zswap_has_pool) {
		 
		if (!strcmp(type, ZSWAP_PARAM_UNSET))
			return NULL;
		if (!strcmp(compressor, ZSWAP_PARAM_UNSET))
			return NULL;
	}

	pool = kzalloc(sizeof(*pool), GFP_KERNEL);
	if (!pool)
		return NULL;

	for (i = 0; i < ZSWAP_NR_ZPOOLS; i++) {
		 
		snprintf(name, 38, "zswap%x",
			 atomic_inc_return(&zswap_pools_count));

		pool->zpools[i] = zpool_create_pool(type, name, gfp);
		if (!pool->zpools[i]) {
			pr_err("%s zpool not available\n", type);
			goto error;
		}
	}
	pr_debug("using %s zpool\n", zpool_get_type(pool->zpools[0]));

	strscpy(pool->tfm_name, compressor, sizeof(pool->tfm_name));

	pool->acomp_ctx = alloc_percpu(*pool->acomp_ctx);
	if (!pool->acomp_ctx) {
		pr_err("percpu alloc failed\n");
		goto error;
	}

	ret = cpuhp_state_add_instance(CPUHP_MM_ZSWP_POOL_PREPARE,
				       &pool->node);
	if (ret)
		goto error;
	pr_debug("using %s compressor\n", pool->tfm_name);

	 
	kref_init(&pool->kref);
	INIT_LIST_HEAD(&pool->list);
	INIT_LIST_HEAD(&pool->lru);
	spin_lock_init(&pool->lru_lock);
	INIT_WORK(&pool->shrink_work, shrink_worker);

	zswap_pool_debug("created", pool);

	return pool;

error:
	if (pool->acomp_ctx)
		free_percpu(pool->acomp_ctx);
	while (i--)
		zpool_destroy_pool(pool->zpools[i]);
	kfree(pool);
	return NULL;
}

static struct zswap_pool *__zswap_pool_create_fallback(void)
{
	bool has_comp, has_zpool;

	has_comp = crypto_has_acomp(zswap_compressor, 0, 0);
	if (!has_comp && strcmp(zswap_compressor,
				CONFIG_ZSWAP_COMPRESSOR_DEFAULT)) {
		pr_err("compressor %s not available, using default %s\n",
		       zswap_compressor, CONFIG_ZSWAP_COMPRESSOR_DEFAULT);
		param_free_charp(&zswap_compressor);
		zswap_compressor = CONFIG_ZSWAP_COMPRESSOR_DEFAULT;
		has_comp = crypto_has_acomp(zswap_compressor, 0, 0);
	}
	if (!has_comp) {
		pr_err("default compressor %s not available\n",
		       zswap_compressor);
		param_free_charp(&zswap_compressor);
		zswap_compressor = ZSWAP_PARAM_UNSET;
	}

	has_zpool = zpool_has_pool(zswap_zpool_type);
	if (!has_zpool && strcmp(zswap_zpool_type,
				 CONFIG_ZSWAP_ZPOOL_DEFAULT)) {
		pr_err("zpool %s not available, using default %s\n",
		       zswap_zpool_type, CONFIG_ZSWAP_ZPOOL_DEFAULT);
		param_free_charp(&zswap_zpool_type);
		zswap_zpool_type = CONFIG_ZSWAP_ZPOOL_DEFAULT;
		has_zpool = zpool_has_pool(zswap_zpool_type);
	}
	if (!has_zpool) {
		pr_err("default zpool %s not available\n",
		       zswap_zpool_type);
		param_free_charp(&zswap_zpool_type);
		zswap_zpool_type = ZSWAP_PARAM_UNSET;
	}

	if (!has_comp || !has_zpool)
		return NULL;

	return zswap_pool_create(zswap_zpool_type, zswap_compressor);
}

static void zswap_pool_destroy(struct zswap_pool *pool)
{
	int i;

	zswap_pool_debug("destroying", pool);

	cpuhp_state_remove_instance(CPUHP_MM_ZSWP_POOL_PREPARE, &pool->node);
	free_percpu(pool->acomp_ctx);
	for (i = 0; i < ZSWAP_NR_ZPOOLS; i++)
		zpool_destroy_pool(pool->zpools[i]);
	kfree(pool);
}

static int __must_check zswap_pool_get(struct zswap_pool *pool)
{
	if (!pool)
		return 0;

	return kref_get_unless_zero(&pool->kref);
}

static void __zswap_pool_release(struct work_struct *work)
{
	struct zswap_pool *pool = container_of(work, typeof(*pool),
						release_work);

	synchronize_rcu();

	 
	WARN_ON(kref_get_unless_zero(&pool->kref));

	 
	zswap_pool_destroy(pool);
}

static void __zswap_pool_empty(struct kref *kref)
{
	struct zswap_pool *pool;

	pool = container_of(kref, typeof(*pool), kref);

	spin_lock(&zswap_pools_lock);

	WARN_ON(pool == zswap_pool_current());

	list_del_rcu(&pool->list);

	INIT_WORK(&pool->release_work, __zswap_pool_release);
	schedule_work(&pool->release_work);

	spin_unlock(&zswap_pools_lock);
}

static void zswap_pool_put(struct zswap_pool *pool)
{
	kref_put(&pool->kref, __zswap_pool_empty);
}

 

static bool zswap_pool_changed(const char *s, const struct kernel_param *kp)
{
	 
	if (!strcmp(s, *(char **)kp->arg) && zswap_has_pool)
		return false;
	return true;
}

 
static int __zswap_param_set(const char *val, const struct kernel_param *kp,
			     char *type, char *compressor)
{
	struct zswap_pool *pool, *put_pool = NULL;
	char *s = strstrip((char *)val);
	int ret = 0;
	bool new_pool = false;

	mutex_lock(&zswap_init_lock);
	switch (zswap_init_state) {
	case ZSWAP_UNINIT:
		 
		ret = param_set_charp(s, kp);
		break;
	case ZSWAP_INIT_SUCCEED:
		new_pool = zswap_pool_changed(s, kp);
		break;
	case ZSWAP_INIT_FAILED:
		pr_err("can't set param, initialization failed\n");
		ret = -ENODEV;
	}
	mutex_unlock(&zswap_init_lock);

	 
	if (!new_pool)
		return ret;

	if (!type) {
		if (!zpool_has_pool(s)) {
			pr_err("zpool %s not available\n", s);
			return -ENOENT;
		}
		type = s;
	} else if (!compressor) {
		if (!crypto_has_acomp(s, 0, 0)) {
			pr_err("compressor %s not available\n", s);
			return -ENOENT;
		}
		compressor = s;
	} else {
		WARN_ON(1);
		return -EINVAL;
	}

	spin_lock(&zswap_pools_lock);

	pool = zswap_pool_find_get(type, compressor);
	if (pool) {
		zswap_pool_debug("using existing", pool);
		WARN_ON(pool == zswap_pool_current());
		list_del_rcu(&pool->list);
	}

	spin_unlock(&zswap_pools_lock);

	if (!pool)
		pool = zswap_pool_create(type, compressor);

	if (pool)
		ret = param_set_charp(s, kp);
	else
		ret = -EINVAL;

	spin_lock(&zswap_pools_lock);

	if (!ret) {
		put_pool = zswap_pool_current();
		list_add_rcu(&pool->list, &zswap_pools);
		zswap_has_pool = true;
	} else if (pool) {
		 
		list_add_tail_rcu(&pool->list, &zswap_pools);
		put_pool = pool;
	}

	spin_unlock(&zswap_pools_lock);

	if (!zswap_has_pool && !pool) {
		 
		ret = param_set_charp(s, kp);
	}

	 
	if (put_pool)
		zswap_pool_put(put_pool);

	return ret;
}

static int zswap_compressor_param_set(const char *val,
				      const struct kernel_param *kp)
{
	return __zswap_param_set(val, kp, zswap_zpool_type, NULL);
}

static int zswap_zpool_param_set(const char *val,
				 const struct kernel_param *kp)
{
	return __zswap_param_set(val, kp, NULL, zswap_compressor);
}

static int zswap_enabled_param_set(const char *val,
				   const struct kernel_param *kp)
{
	int ret = -ENODEV;

	 
	if (system_state != SYSTEM_RUNNING)
		return param_set_bool(val, kp);

	mutex_lock(&zswap_init_lock);
	switch (zswap_init_state) {
	case ZSWAP_UNINIT:
		if (zswap_setup())
			break;
		fallthrough;
	case ZSWAP_INIT_SUCCEED:
		if (!zswap_has_pool)
			pr_err("can't enable, no pool configured\n");
		else
			ret = param_set_bool(val, kp);
		break;
	case ZSWAP_INIT_FAILED:
		pr_err("can't enable, initialization failed\n");
	}
	mutex_unlock(&zswap_init_lock);

	return ret;
}

 
 
static int zswap_writeback_entry(struct zswap_entry *entry,
				 struct zswap_tree *tree)
{
	swp_entry_t swpentry = entry->swpentry;
	struct page *page;
	struct scatterlist input, output;
	struct crypto_acomp_ctx *acomp_ctx;
	struct zpool *pool = zswap_find_zpool(entry);
	bool page_was_allocated;
	u8 *src, *tmp = NULL;
	unsigned int dlen;
	int ret;
	struct writeback_control wbc = {
		.sync_mode = WB_SYNC_NONE,
	};

	if (!zpool_can_sleep_mapped(pool)) {
		tmp = kmalloc(PAGE_SIZE, GFP_KERNEL);
		if (!tmp)
			return -ENOMEM;
	}

	 
	page = __read_swap_cache_async(swpentry, GFP_KERNEL, NULL, 0,
				       &page_was_allocated);
	if (!page) {
		ret = -ENOMEM;
		goto fail;
	}

	 
	if (!page_was_allocated) {
		put_page(page);
		ret = -EEXIST;
		goto fail;
	}

	 
	spin_lock(&tree->lock);
	if (zswap_rb_search(&tree->rbroot, swp_offset(entry->swpentry)) != entry) {
		spin_unlock(&tree->lock);
		delete_from_swap_cache(page_folio(page));
		ret = -ENOMEM;
		goto fail;
	}
	spin_unlock(&tree->lock);

	 
	acomp_ctx = raw_cpu_ptr(entry->pool->acomp_ctx);
	dlen = PAGE_SIZE;

	src = zpool_map_handle(pool, entry->handle, ZPOOL_MM_RO);
	if (!zpool_can_sleep_mapped(pool)) {
		memcpy(tmp, src, entry->length);
		src = tmp;
		zpool_unmap_handle(pool, entry->handle);
	}

	mutex_lock(acomp_ctx->mutex);
	sg_init_one(&input, src, entry->length);
	sg_init_table(&output, 1);
	sg_set_page(&output, page, PAGE_SIZE, 0);
	acomp_request_set_params(acomp_ctx->req, &input, &output, entry->length, dlen);
	ret = crypto_wait_req(crypto_acomp_decompress(acomp_ctx->req), &acomp_ctx->wait);
	dlen = acomp_ctx->req->dlen;
	mutex_unlock(acomp_ctx->mutex);

	if (!zpool_can_sleep_mapped(pool))
		kfree(tmp);
	else
		zpool_unmap_handle(pool, entry->handle);

	BUG_ON(ret);
	BUG_ON(dlen != PAGE_SIZE);

	 
	SetPageUptodate(page);

	 
	SetPageReclaim(page);

	 
	__swap_writepage(page, &wbc);
	put_page(page);
	zswap_written_back_pages++;

	return ret;

fail:
	if (!zpool_can_sleep_mapped(pool))
		kfree(tmp);

	 
	return ret;
}

static int zswap_is_page_same_filled(void *ptr, unsigned long *value)
{
	unsigned long *page;
	unsigned long val;
	unsigned int pos, last_pos = PAGE_SIZE / sizeof(*page) - 1;

	page = (unsigned long *)ptr;
	val = page[0];

	if (val != page[last_pos])
		return 0;

	for (pos = 1; pos < last_pos; pos++) {
		if (val != page[pos])
			return 0;
	}

	*value = val;

	return 1;
}

static void zswap_fill_page(void *ptr, unsigned long value)
{
	unsigned long *page;

	page = (unsigned long *)ptr;
	memset_l(page, value, PAGE_SIZE / sizeof(unsigned long));
}

bool zswap_store(struct folio *folio)
{
	swp_entry_t swp = folio->swap;
	int type = swp_type(swp);
	pgoff_t offset = swp_offset(swp);
	struct page *page = &folio->page;
	struct zswap_tree *tree = zswap_trees[type];
	struct zswap_entry *entry, *dupentry;
	struct scatterlist input, output;
	struct crypto_acomp_ctx *acomp_ctx;
	struct obj_cgroup *objcg = NULL;
	struct zswap_pool *pool;
	struct zpool *zpool;
	unsigned int dlen = PAGE_SIZE;
	unsigned long handle, value;
	char *buf;
	u8 *src, *dst;
	gfp_t gfp;
	int ret;

	VM_WARN_ON_ONCE(!folio_test_locked(folio));
	VM_WARN_ON_ONCE(!folio_test_swapcache(folio));

	 
	if (folio_test_large(folio))
		return false;

	if (!zswap_enabled || !tree)
		return false;

	 
	spin_lock(&tree->lock);
	dupentry = zswap_rb_search(&tree->rbroot, offset);
	if (dupentry) {
		zswap_duplicate_entry++;
		zswap_invalidate_entry(tree, dupentry);
	}
	spin_unlock(&tree->lock);

	 
	objcg = get_obj_cgroup_from_folio(folio);
	if (objcg && !obj_cgroup_may_zswap(objcg))
		goto reject;

	 
	if (zswap_is_full()) {
		zswap_pool_limit_hit++;
		zswap_pool_reached_full = true;
		goto shrink;
	}

	if (zswap_pool_reached_full) {
	       if (!zswap_can_accept())
			goto shrink;
		else
			zswap_pool_reached_full = false;
	}

	 
	entry = zswap_entry_cache_alloc(GFP_KERNEL);
	if (!entry) {
		zswap_reject_kmemcache_fail++;
		goto reject;
	}

	if (zswap_same_filled_pages_enabled) {
		src = kmap_atomic(page);
		if (zswap_is_page_same_filled(src, &value)) {
			kunmap_atomic(src);
			entry->swpentry = swp_entry(type, offset);
			entry->length = 0;
			entry->value = value;
			atomic_inc(&zswap_same_filled_pages);
			goto insert_entry;
		}
		kunmap_atomic(src);
	}

	if (!zswap_non_same_filled_pages_enabled)
		goto freepage;

	 
	entry->pool = zswap_pool_current_get();
	if (!entry->pool)
		goto freepage;

	 
	acomp_ctx = raw_cpu_ptr(entry->pool->acomp_ctx);

	mutex_lock(acomp_ctx->mutex);

	dst = acomp_ctx->dstmem;
	sg_init_table(&input, 1);
	sg_set_page(&input, page, PAGE_SIZE, 0);

	 
	sg_init_one(&output, dst, PAGE_SIZE * 2);
	acomp_request_set_params(acomp_ctx->req, &input, &output, PAGE_SIZE, dlen);
	 
	ret = crypto_wait_req(crypto_acomp_compress(acomp_ctx->req), &acomp_ctx->wait);
	dlen = acomp_ctx->req->dlen;

	if (ret)
		goto put_dstmem;

	 
	zpool = zswap_find_zpool(entry);
	gfp = __GFP_NORETRY | __GFP_NOWARN | __GFP_KSWAPD_RECLAIM;
	if (zpool_malloc_support_movable(zpool))
		gfp |= __GFP_HIGHMEM | __GFP_MOVABLE;
	ret = zpool_malloc(zpool, dlen, gfp, &handle);
	if (ret == -ENOSPC) {
		zswap_reject_compress_poor++;
		goto put_dstmem;
	}
	if (ret) {
		zswap_reject_alloc_fail++;
		goto put_dstmem;
	}
	buf = zpool_map_handle(zpool, handle, ZPOOL_MM_WO);
	memcpy(buf, dst, dlen);
	zpool_unmap_handle(zpool, handle);
	mutex_unlock(acomp_ctx->mutex);

	 
	entry->swpentry = swp_entry(type, offset);
	entry->handle = handle;
	entry->length = dlen;

insert_entry:
	entry->objcg = objcg;
	if (objcg) {
		obj_cgroup_charge_zswap(objcg, entry->length);
		 
		count_objcg_event(objcg, ZSWPOUT);
	}

	 
	spin_lock(&tree->lock);
	 
	while (zswap_rb_insert(&tree->rbroot, entry, &dupentry) == -EEXIST) {
		WARN_ON(1);
		zswap_duplicate_entry++;
		zswap_invalidate_entry(tree, dupentry);
	}
	if (entry->length) {
		spin_lock(&entry->pool->lru_lock);
		list_add(&entry->lru, &entry->pool->lru);
		spin_unlock(&entry->pool->lru_lock);
	}
	spin_unlock(&tree->lock);

	 
	atomic_inc(&zswap_stored_pages);
	zswap_update_total_size();
	count_vm_event(ZSWPOUT);

	return true;

put_dstmem:
	mutex_unlock(acomp_ctx->mutex);
	zswap_pool_put(entry->pool);
freepage:
	zswap_entry_cache_free(entry);
reject:
	if (objcg)
		obj_cgroup_put(objcg);
	return false;

shrink:
	pool = zswap_pool_last_get();
	if (pool && !queue_work(shrink_wq, &pool->shrink_work))
		zswap_pool_put(pool);
	goto reject;
}

bool zswap_load(struct folio *folio)
{
	swp_entry_t swp = folio->swap;
	int type = swp_type(swp);
	pgoff_t offset = swp_offset(swp);
	struct page *page = &folio->page;
	struct zswap_tree *tree = zswap_trees[type];
	struct zswap_entry *entry;
	struct scatterlist input, output;
	struct crypto_acomp_ctx *acomp_ctx;
	u8 *src, *dst, *tmp;
	struct zpool *zpool;
	unsigned int dlen;
	bool ret;

	VM_WARN_ON_ONCE(!folio_test_locked(folio));

	 
	spin_lock(&tree->lock);
	entry = zswap_entry_find_get(&tree->rbroot, offset);
	if (!entry) {
		spin_unlock(&tree->lock);
		return false;
	}
	spin_unlock(&tree->lock);

	if (!entry->length) {
		dst = kmap_atomic(page);
		zswap_fill_page(dst, entry->value);
		kunmap_atomic(dst);
		ret = true;
		goto stats;
	}

	zpool = zswap_find_zpool(entry);
	if (!zpool_can_sleep_mapped(zpool)) {
		tmp = kmalloc(entry->length, GFP_KERNEL);
		if (!tmp) {
			ret = false;
			goto freeentry;
		}
	}

	 
	dlen = PAGE_SIZE;
	src = zpool_map_handle(zpool, entry->handle, ZPOOL_MM_RO);

	if (!zpool_can_sleep_mapped(zpool)) {
		memcpy(tmp, src, entry->length);
		src = tmp;
		zpool_unmap_handle(zpool, entry->handle);
	}

	acomp_ctx = raw_cpu_ptr(entry->pool->acomp_ctx);
	mutex_lock(acomp_ctx->mutex);
	sg_init_one(&input, src, entry->length);
	sg_init_table(&output, 1);
	sg_set_page(&output, page, PAGE_SIZE, 0);
	acomp_request_set_params(acomp_ctx->req, &input, &output, entry->length, dlen);
	if (crypto_wait_req(crypto_acomp_decompress(acomp_ctx->req), &acomp_ctx->wait))
		WARN_ON(1);
	mutex_unlock(acomp_ctx->mutex);

	if (zpool_can_sleep_mapped(zpool))
		zpool_unmap_handle(zpool, entry->handle);
	else
		kfree(tmp);

	ret = true;
stats:
	count_vm_event(ZSWPIN);
	if (entry->objcg)
		count_objcg_event(entry->objcg, ZSWPIN);
freeentry:
	spin_lock(&tree->lock);
	if (ret && zswap_exclusive_loads_enabled) {
		zswap_invalidate_entry(tree, entry);
		folio_mark_dirty(folio);
	} else if (entry->length) {
		spin_lock(&entry->pool->lru_lock);
		list_move(&entry->lru, &entry->pool->lru);
		spin_unlock(&entry->pool->lru_lock);
	}
	zswap_entry_put(tree, entry);
	spin_unlock(&tree->lock);

	return ret;
}

void zswap_invalidate(int type, pgoff_t offset)
{
	struct zswap_tree *tree = zswap_trees[type];
	struct zswap_entry *entry;

	 
	spin_lock(&tree->lock);
	entry = zswap_rb_search(&tree->rbroot, offset);
	if (!entry) {
		 
		spin_unlock(&tree->lock);
		return;
	}
	zswap_invalidate_entry(tree, entry);
	spin_unlock(&tree->lock);
}

void zswap_swapon(int type)
{
	struct zswap_tree *tree;

	tree = kzalloc(sizeof(*tree), GFP_KERNEL);
	if (!tree) {
		pr_err("alloc failed, zswap disabled for swap type %d\n", type);
		return;
	}

	tree->rbroot = RB_ROOT;
	spin_lock_init(&tree->lock);
	zswap_trees[type] = tree;
}

void zswap_swapoff(int type)
{
	struct zswap_tree *tree = zswap_trees[type];
	struct zswap_entry *entry, *n;

	if (!tree)
		return;

	 
	spin_lock(&tree->lock);
	rbtree_postorder_for_each_entry_safe(entry, n, &tree->rbroot, rbnode)
		zswap_free_entry(entry);
	tree->rbroot = RB_ROOT;
	spin_unlock(&tree->lock);
	kfree(tree);
	zswap_trees[type] = NULL;
}

 
#ifdef CONFIG_DEBUG_FS
#include <linux/debugfs.h>

static struct dentry *zswap_debugfs_root;

static int zswap_debugfs_init(void)
{
	if (!debugfs_initialized())
		return -ENODEV;

	zswap_debugfs_root = debugfs_create_dir("zswap", NULL);

	debugfs_create_u64("pool_limit_hit", 0444,
			   zswap_debugfs_root, &zswap_pool_limit_hit);
	debugfs_create_u64("reject_reclaim_fail", 0444,
			   zswap_debugfs_root, &zswap_reject_reclaim_fail);
	debugfs_create_u64("reject_alloc_fail", 0444,
			   zswap_debugfs_root, &zswap_reject_alloc_fail);
	debugfs_create_u64("reject_kmemcache_fail", 0444,
			   zswap_debugfs_root, &zswap_reject_kmemcache_fail);
	debugfs_create_u64("reject_compress_poor", 0444,
			   zswap_debugfs_root, &zswap_reject_compress_poor);
	debugfs_create_u64("written_back_pages", 0444,
			   zswap_debugfs_root, &zswap_written_back_pages);
	debugfs_create_u64("duplicate_entry", 0444,
			   zswap_debugfs_root, &zswap_duplicate_entry);
	debugfs_create_u64("pool_total_size", 0444,
			   zswap_debugfs_root, &zswap_pool_total_size);
	debugfs_create_atomic_t("stored_pages", 0444,
				zswap_debugfs_root, &zswap_stored_pages);
	debugfs_create_atomic_t("same_filled_pages", 0444,
				zswap_debugfs_root, &zswap_same_filled_pages);

	return 0;
}
#else
static int zswap_debugfs_init(void)
{
	return 0;
}
#endif

 
static int zswap_setup(void)
{
	struct zswap_pool *pool;
	int ret;

	zswap_entry_cache = KMEM_CACHE(zswap_entry, 0);
	if (!zswap_entry_cache) {
		pr_err("entry cache creation failed\n");
		goto cache_fail;
	}

	ret = cpuhp_setup_state(CPUHP_MM_ZSWP_MEM_PREPARE, "mm/zswap:prepare",
				zswap_dstmem_prepare, zswap_dstmem_dead);
	if (ret) {
		pr_err("dstmem alloc failed\n");
		goto dstmem_fail;
	}

	ret = cpuhp_setup_state_multi(CPUHP_MM_ZSWP_POOL_PREPARE,
				      "mm/zswap_pool:prepare",
				      zswap_cpu_comp_prepare,
				      zswap_cpu_comp_dead);
	if (ret)
		goto hp_fail;

	pool = __zswap_pool_create_fallback();
	if (pool) {
		pr_info("loaded using pool %s/%s\n", pool->tfm_name,
			zpool_get_type(pool->zpools[0]));
		list_add(&pool->list, &zswap_pools);
		zswap_has_pool = true;
	} else {
		pr_err("pool creation failed\n");
		zswap_enabled = false;
	}

	shrink_wq = create_workqueue("zswap-shrink");
	if (!shrink_wq)
		goto fallback_fail;

	if (zswap_debugfs_init())
		pr_warn("debugfs initialization failed\n");
	zswap_init_state = ZSWAP_INIT_SUCCEED;
	return 0;

fallback_fail:
	if (pool)
		zswap_pool_destroy(pool);
hp_fail:
	cpuhp_remove_state(CPUHP_MM_ZSWP_MEM_PREPARE);
dstmem_fail:
	kmem_cache_destroy(zswap_entry_cache);
cache_fail:
	 
	zswap_init_state = ZSWAP_INIT_FAILED;
	zswap_enabled = false;
	return -ENOMEM;
}

static int __init zswap_init(void)
{
	if (!zswap_enabled)
		return 0;
	return zswap_setup();
}
 
late_initcall(zswap_init);

MODULE_AUTHOR("Seth Jennings <sjennings@variantweb.net>");
MODULE_DESCRIPTION("Compressed cache for swap pages");
