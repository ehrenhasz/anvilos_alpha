#ifndef MM_SLAB_H
#define MM_SLAB_H
void __init kmem_cache_init(void);
#ifdef CONFIG_64BIT
# ifdef system_has_cmpxchg128
# define system_has_freelist_aba()	system_has_cmpxchg128()
# define try_cmpxchg_freelist		try_cmpxchg128
# endif
#define this_cpu_try_cmpxchg_freelist	this_cpu_try_cmpxchg128
typedef u128 freelist_full_t;
#else  
# ifdef system_has_cmpxchg64
# define system_has_freelist_aba()	system_has_cmpxchg64()
# define try_cmpxchg_freelist		try_cmpxchg64
# endif
#define this_cpu_try_cmpxchg_freelist	this_cpu_try_cmpxchg64
typedef u64 freelist_full_t;
#endif  
#if defined(system_has_freelist_aba) && !defined(CONFIG_HAVE_ALIGNED_STRUCT_PAGE)
#undef system_has_freelist_aba
#endif
typedef union {
	struct {
		void *freelist;
		unsigned long counter;
	};
	freelist_full_t full;
} freelist_aba_t;
struct slab {
	unsigned long __page_flags;
#if defined(CONFIG_SLAB)
	struct kmem_cache *slab_cache;
	union {
		struct {
			struct list_head slab_list;
			void *freelist;	 
			void *s_mem;	 
		};
		struct rcu_head rcu_head;
	};
	unsigned int active;
#elif defined(CONFIG_SLUB)
	struct kmem_cache *slab_cache;
	union {
		struct {
			union {
				struct list_head slab_list;
#ifdef CONFIG_SLUB_CPU_PARTIAL
				struct {
					struct slab *next;
					int slabs;	 
				};
#endif
			};
			union {
				struct {
					void *freelist;		 
					union {
						unsigned long counters;
						struct {
							unsigned inuse:16;
							unsigned objects:15;
							unsigned frozen:1;
						};
					};
				};
#ifdef system_has_freelist_aba
				freelist_aba_t freelist_counter;
#endif
			};
		};
		struct rcu_head rcu_head;
	};
	unsigned int __unused;
#else
#error "Unexpected slab allocator configured"
#endif
	atomic_t __page_refcount;
#ifdef CONFIG_MEMCG
	unsigned long memcg_data;
#endif
};
#define SLAB_MATCH(pg, sl)						\
	static_assert(offsetof(struct page, pg) == offsetof(struct slab, sl))
SLAB_MATCH(flags, __page_flags);
SLAB_MATCH(compound_head, slab_cache);	 
SLAB_MATCH(_refcount, __page_refcount);
#ifdef CONFIG_MEMCG
SLAB_MATCH(memcg_data, memcg_data);
#endif
#undef SLAB_MATCH
static_assert(sizeof(struct slab) <= sizeof(struct page));
#if defined(system_has_freelist_aba) && defined(CONFIG_SLUB)
static_assert(IS_ALIGNED(offsetof(struct slab, freelist), sizeof(freelist_aba_t)));
#endif
#define folio_slab(folio)	(_Generic((folio),			\
	const struct folio *:	(const struct slab *)(folio),		\
	struct folio *:		(struct slab *)(folio)))
#define slab_folio(s)		(_Generic((s),				\
	const struct slab *:	(const struct folio *)s,		\
	struct slab *:		(struct folio *)s))
#define page_slab(p)		(_Generic((p),				\
	const struct page *:	(const struct slab *)(p),		\
	struct page *:		(struct slab *)(p)))
#define slab_page(s) folio_page(slab_folio(s), 0)
static inline bool slab_test_pfmemalloc(const struct slab *slab)
{
	return folio_test_active((struct folio *)slab_folio(slab));
}
static inline void slab_set_pfmemalloc(struct slab *slab)
{
	folio_set_active(slab_folio(slab));
}
static inline void slab_clear_pfmemalloc(struct slab *slab)
{
	folio_clear_active(slab_folio(slab));
}
static inline void __slab_clear_pfmemalloc(struct slab *slab)
{
	__folio_clear_active(slab_folio(slab));
}
static inline void *slab_address(const struct slab *slab)
{
	return folio_address(slab_folio(slab));
}
static inline int slab_nid(const struct slab *slab)
{
	return folio_nid(slab_folio(slab));
}
static inline pg_data_t *slab_pgdat(const struct slab *slab)
{
	return folio_pgdat(slab_folio(slab));
}
static inline struct slab *virt_to_slab(const void *addr)
{
	struct folio *folio = virt_to_folio(addr);
	if (!folio_test_slab(folio))
		return NULL;
	return folio_slab(folio);
}
static inline int slab_order(const struct slab *slab)
{
	return folio_order((struct folio *)slab_folio(slab));
}
static inline size_t slab_size(const struct slab *slab)
{
	return PAGE_SIZE << slab_order(slab);
}
#ifdef CONFIG_SLAB
#include <linux/slab_def.h>
#endif
#ifdef CONFIG_SLUB
#include <linux/slub_def.h>
#endif
#include <linux/memcontrol.h>
#include <linux/fault-inject.h>
#include <linux/kasan.h>
#include <linux/kmemleak.h>
#include <linux/random.h>
#include <linux/sched/mm.h>
#include <linux/list_lru.h>
enum slab_state {
	DOWN,			 
	PARTIAL,		 
	PARTIAL_NODE,		 
	UP,			 
	FULL			 
};
extern enum slab_state slab_state;
extern struct mutex slab_mutex;
extern struct list_head slab_caches;
extern struct kmem_cache *kmem_cache;
extern const struct kmalloc_info_struct {
	const char *name[NR_KMALLOC_TYPES];
	unsigned int size;
} kmalloc_info[];
void setup_kmalloc_cache_index_table(void);
void create_kmalloc_caches(slab_flags_t);
struct kmem_cache *kmalloc_slab(size_t size, gfp_t flags, unsigned long caller);
void *__kmem_cache_alloc_node(struct kmem_cache *s, gfp_t gfpflags,
			      int node, size_t orig_size,
			      unsigned long caller);
void __kmem_cache_free(struct kmem_cache *s, void *x, unsigned long caller);
gfp_t kmalloc_fix_flags(gfp_t flags);
int __kmem_cache_create(struct kmem_cache *, slab_flags_t flags);
void __init new_kmalloc_cache(int idx, enum kmalloc_cache_type type,
			      slab_flags_t flags);
extern void create_boot_cache(struct kmem_cache *, const char *name,
			unsigned int size, slab_flags_t flags,
			unsigned int useroffset, unsigned int usersize);
int slab_unmergeable(struct kmem_cache *s);
struct kmem_cache *find_mergeable(unsigned size, unsigned align,
		slab_flags_t flags, const char *name, void (*ctor)(void *));
struct kmem_cache *
__kmem_cache_alias(const char *name, unsigned int size, unsigned int align,
		   slab_flags_t flags, void (*ctor)(void *));
slab_flags_t kmem_cache_flags(unsigned int object_size,
	slab_flags_t flags, const char *name);
static inline bool is_kmalloc_cache(struct kmem_cache *s)
{
	return (s->flags & SLAB_KMALLOC);
}
#define SLAB_CORE_FLAGS (SLAB_HWCACHE_ALIGN | SLAB_CACHE_DMA | \
			 SLAB_CACHE_DMA32 | SLAB_PANIC | \
			 SLAB_TYPESAFE_BY_RCU | SLAB_DEBUG_OBJECTS )
#if defined(CONFIG_DEBUG_SLAB)
#define SLAB_DEBUG_FLAGS (SLAB_RED_ZONE | SLAB_POISON | SLAB_STORE_USER)
#elif defined(CONFIG_SLUB_DEBUG)
#define SLAB_DEBUG_FLAGS (SLAB_RED_ZONE | SLAB_POISON | SLAB_STORE_USER | \
			  SLAB_TRACE | SLAB_CONSISTENCY_CHECKS)
#else
#define SLAB_DEBUG_FLAGS (0)
#endif
#if defined(CONFIG_SLAB)
#define SLAB_CACHE_FLAGS (SLAB_MEM_SPREAD | SLAB_NOLEAKTRACE | \
			  SLAB_RECLAIM_ACCOUNT | SLAB_TEMPORARY | \
			  SLAB_ACCOUNT | SLAB_NO_MERGE)
#elif defined(CONFIG_SLUB)
#define SLAB_CACHE_FLAGS (SLAB_NOLEAKTRACE | SLAB_RECLAIM_ACCOUNT | \
			  SLAB_TEMPORARY | SLAB_ACCOUNT | \
			  SLAB_NO_USER_FLAGS | SLAB_KMALLOC | SLAB_NO_MERGE)
#else
#define SLAB_CACHE_FLAGS (SLAB_NOLEAKTRACE)
#endif
#define CACHE_CREATE_MASK (SLAB_CORE_FLAGS | SLAB_DEBUG_FLAGS | SLAB_CACHE_FLAGS)
#define SLAB_FLAGS_PERMITTED (SLAB_CORE_FLAGS | \
			      SLAB_RED_ZONE | \
			      SLAB_POISON | \
			      SLAB_STORE_USER | \
			      SLAB_TRACE | \
			      SLAB_CONSISTENCY_CHECKS | \
			      SLAB_MEM_SPREAD | \
			      SLAB_NOLEAKTRACE | \
			      SLAB_RECLAIM_ACCOUNT | \
			      SLAB_TEMPORARY | \
			      SLAB_ACCOUNT | \
			      SLAB_KMALLOC | \
			      SLAB_NO_MERGE | \
			      SLAB_NO_USER_FLAGS)
bool __kmem_cache_empty(struct kmem_cache *);
int __kmem_cache_shutdown(struct kmem_cache *);
void __kmem_cache_release(struct kmem_cache *);
int __kmem_cache_shrink(struct kmem_cache *);
void slab_kmem_cache_release(struct kmem_cache *);
struct seq_file;
struct file;
struct slabinfo {
	unsigned long active_objs;
	unsigned long num_objs;
	unsigned long active_slabs;
	unsigned long num_slabs;
	unsigned long shared_avail;
	unsigned int limit;
	unsigned int batchcount;
	unsigned int shared;
	unsigned int objects_per_slab;
	unsigned int cache_order;
};
void get_slabinfo(struct kmem_cache *s, struct slabinfo *sinfo);
void slabinfo_show_stats(struct seq_file *m, struct kmem_cache *s);
ssize_t slabinfo_write(struct file *file, const char __user *buffer,
		       size_t count, loff_t *ppos);
static inline enum node_stat_item cache_vmstat_idx(struct kmem_cache *s)
{
	return (s->flags & SLAB_RECLAIM_ACCOUNT) ?
		NR_SLAB_RECLAIMABLE_B : NR_SLAB_UNRECLAIMABLE_B;
}
#ifdef CONFIG_SLUB_DEBUG
#ifdef CONFIG_SLUB_DEBUG_ON
DECLARE_STATIC_KEY_TRUE(slub_debug_enabled);
#else
DECLARE_STATIC_KEY_FALSE(slub_debug_enabled);
#endif
extern void print_tracking(struct kmem_cache *s, void *object);
long validate_slab_cache(struct kmem_cache *s);
static inline bool __slub_debug_enabled(void)
{
	return static_branch_unlikely(&slub_debug_enabled);
}
#else
static inline void print_tracking(struct kmem_cache *s, void *object)
{
}
static inline bool __slub_debug_enabled(void)
{
	return false;
}
#endif
static inline bool kmem_cache_debug_flags(struct kmem_cache *s, slab_flags_t flags)
{
	if (IS_ENABLED(CONFIG_SLUB_DEBUG))
		VM_WARN_ON_ONCE(!(flags & SLAB_DEBUG_FLAGS));
	if (__slub_debug_enabled())
		return s->flags & flags;
	return false;
}
#ifdef CONFIG_MEMCG_KMEM
static inline struct obj_cgroup **slab_objcgs(struct slab *slab)
{
	unsigned long memcg_data = READ_ONCE(slab->memcg_data);
	VM_BUG_ON_PAGE(memcg_data && !(memcg_data & MEMCG_DATA_OBJCGS),
							slab_page(slab));
	VM_BUG_ON_PAGE(memcg_data & MEMCG_DATA_KMEM, slab_page(slab));
	return (struct obj_cgroup **)(memcg_data & ~MEMCG_DATA_FLAGS_MASK);
}
int memcg_alloc_slab_cgroups(struct slab *slab, struct kmem_cache *s,
				 gfp_t gfp, bool new_slab);
void mod_objcg_state(struct obj_cgroup *objcg, struct pglist_data *pgdat,
		     enum node_stat_item idx, int nr);
static inline void memcg_free_slab_cgroups(struct slab *slab)
{
	kfree(slab_objcgs(slab));
	slab->memcg_data = 0;
}
static inline size_t obj_full_size(struct kmem_cache *s)
{
	return s->size + sizeof(struct obj_cgroup *);
}
static inline bool memcg_slab_pre_alloc_hook(struct kmem_cache *s,
					     struct list_lru *lru,
					     struct obj_cgroup **objcgp,
					     size_t objects, gfp_t flags)
{
	struct obj_cgroup *objcg;
	if (!memcg_kmem_online())
		return true;
	if (!(flags & __GFP_ACCOUNT) && !(s->flags & SLAB_ACCOUNT))
		return true;
	objcg = get_obj_cgroup_from_current();
	if (!objcg)
		return true;
	if (lru) {
		int ret;
		struct mem_cgroup *memcg;
		memcg = get_mem_cgroup_from_objcg(objcg);
		ret = memcg_list_lru_alloc(memcg, lru, flags);
		css_put(&memcg->css);
		if (ret)
			goto out;
	}
	if (obj_cgroup_charge(objcg, flags, objects * obj_full_size(s)))
		goto out;
	*objcgp = objcg;
	return true;
out:
	obj_cgroup_put(objcg);
	return false;
}
static inline void memcg_slab_post_alloc_hook(struct kmem_cache *s,
					      struct obj_cgroup *objcg,
					      gfp_t flags, size_t size,
					      void **p)
{
	struct slab *slab;
	unsigned long off;
	size_t i;
	if (!memcg_kmem_online() || !objcg)
		return;
	for (i = 0; i < size; i++) {
		if (likely(p[i])) {
			slab = virt_to_slab(p[i]);
			if (!slab_objcgs(slab) &&
			    memcg_alloc_slab_cgroups(slab, s, flags,
							 false)) {
				obj_cgroup_uncharge(objcg, obj_full_size(s));
				continue;
			}
			off = obj_to_index(s, slab, p[i]);
			obj_cgroup_get(objcg);
			slab_objcgs(slab)[off] = objcg;
			mod_objcg_state(objcg, slab_pgdat(slab),
					cache_vmstat_idx(s), obj_full_size(s));
		} else {
			obj_cgroup_uncharge(objcg, obj_full_size(s));
		}
	}
	obj_cgroup_put(objcg);
}
static inline void memcg_slab_free_hook(struct kmem_cache *s, struct slab *slab,
					void **p, int objects)
{
	struct obj_cgroup **objcgs;
	int i;
	if (!memcg_kmem_online())
		return;
	objcgs = slab_objcgs(slab);
	if (!objcgs)
		return;
	for (i = 0; i < objects; i++) {
		struct obj_cgroup *objcg;
		unsigned int off;
		off = obj_to_index(s, slab, p[i]);
		objcg = objcgs[off];
		if (!objcg)
			continue;
		objcgs[off] = NULL;
		obj_cgroup_uncharge(objcg, obj_full_size(s));
		mod_objcg_state(objcg, slab_pgdat(slab), cache_vmstat_idx(s),
				-obj_full_size(s));
		obj_cgroup_put(objcg);
	}
}
#else  
static inline struct obj_cgroup **slab_objcgs(struct slab *slab)
{
	return NULL;
}
static inline struct mem_cgroup *memcg_from_slab_obj(void *ptr)
{
	return NULL;
}
static inline int memcg_alloc_slab_cgroups(struct slab *slab,
					       struct kmem_cache *s, gfp_t gfp,
					       bool new_slab)
{
	return 0;
}
static inline void memcg_free_slab_cgroups(struct slab *slab)
{
}
static inline bool memcg_slab_pre_alloc_hook(struct kmem_cache *s,
					     struct list_lru *lru,
					     struct obj_cgroup **objcgp,
					     size_t objects, gfp_t flags)
{
	return true;
}
static inline void memcg_slab_post_alloc_hook(struct kmem_cache *s,
					      struct obj_cgroup *objcg,
					      gfp_t flags, size_t size,
					      void **p)
{
}
static inline void memcg_slab_free_hook(struct kmem_cache *s, struct slab *slab,
					void **p, int objects)
{
}
#endif  
static inline struct kmem_cache *virt_to_cache(const void *obj)
{
	struct slab *slab;
	slab = virt_to_slab(obj);
	if (WARN_ONCE(!slab, "%s: Object is not a Slab page!\n",
					__func__))
		return NULL;
	return slab->slab_cache;
}
static __always_inline void account_slab(struct slab *slab, int order,
					 struct kmem_cache *s, gfp_t gfp)
{
	if (memcg_kmem_online() && (s->flags & SLAB_ACCOUNT))
		memcg_alloc_slab_cgroups(slab, s, gfp, true);
	mod_node_page_state(slab_pgdat(slab), cache_vmstat_idx(s),
			    PAGE_SIZE << order);
}
static __always_inline void unaccount_slab(struct slab *slab, int order,
					   struct kmem_cache *s)
{
	if (memcg_kmem_online())
		memcg_free_slab_cgroups(slab);
	mod_node_page_state(slab_pgdat(slab), cache_vmstat_idx(s),
			    -(PAGE_SIZE << order));
}
static inline struct kmem_cache *cache_from_obj(struct kmem_cache *s, void *x)
{
	struct kmem_cache *cachep;
	if (!IS_ENABLED(CONFIG_SLAB_FREELIST_HARDENED) &&
	    !kmem_cache_debug_flags(s, SLAB_CONSISTENCY_CHECKS))
		return s;
	cachep = virt_to_cache(x);
	if (WARN(cachep && cachep != s,
		  "%s: Wrong slab cache. %s but object is from %s\n",
		  __func__, s->name, cachep->name))
		print_tracking(cachep, x);
	return cachep;
}
void free_large_kmalloc(struct folio *folio, void *object);
size_t __ksize(const void *objp);
static inline size_t slab_ksize(const struct kmem_cache *s)
{
#ifndef CONFIG_SLUB
	return s->object_size;
#else  
# ifdef CONFIG_SLUB_DEBUG
	if (s->flags & (SLAB_RED_ZONE | SLAB_POISON))
		return s->object_size;
# endif
	if (s->flags & SLAB_KASAN)
		return s->object_size;
	if (s->flags & (SLAB_TYPESAFE_BY_RCU | SLAB_STORE_USER))
		return s->inuse;
	return s->size;
#endif
}
static inline struct kmem_cache *slab_pre_alloc_hook(struct kmem_cache *s,
						     struct list_lru *lru,
						     struct obj_cgroup **objcgp,
						     size_t size, gfp_t flags)
{
	flags &= gfp_allowed_mask;
	might_alloc(flags);
	if (should_failslab(s, flags))
		return NULL;
	if (!memcg_slab_pre_alloc_hook(s, lru, objcgp, size, flags))
		return NULL;
	return s;
}
static inline void slab_post_alloc_hook(struct kmem_cache *s,
					struct obj_cgroup *objcg, gfp_t flags,
					size_t size, void **p, bool init,
					unsigned int orig_size)
{
	unsigned int zero_size = s->object_size;
	bool kasan_init = init;
	size_t i;
	flags &= gfp_allowed_mask;
	if (kmem_cache_debug_flags(s, SLAB_STORE_USER | SLAB_RED_ZONE) &&
	    (s->flags & SLAB_KMALLOC))
		zero_size = orig_size;
	if (__slub_debug_enabled())
		kasan_init = false;
	for (i = 0; i < size; i++) {
		p[i] = kasan_slab_alloc(s, p[i], flags, kasan_init);
		if (p[i] && init && (!kasan_init || !kasan_has_integrated_init()))
			memset(p[i], 0, zero_size);
		kmemleak_alloc_recursive(p[i], s->object_size, 1,
					 s->flags, flags);
		kmsan_slab_alloc(s, p[i], flags);
	}
	memcg_slab_post_alloc_hook(s, objcg, flags, size, p);
}
struct kmem_cache_node {
#ifdef CONFIG_SLAB
	raw_spinlock_t list_lock;
	struct list_head slabs_partial;	 
	struct list_head slabs_full;
	struct list_head slabs_free;
	unsigned long total_slabs;	 
	unsigned long free_slabs;	 
	unsigned long free_objects;
	unsigned int free_limit;
	unsigned int colour_next;	 
	struct array_cache *shared;	 
	struct alien_cache **alien;	 
	unsigned long next_reap;	 
	int free_touched;		 
#endif
#ifdef CONFIG_SLUB
	spinlock_t list_lock;
	unsigned long nr_partial;
	struct list_head partial;
#ifdef CONFIG_SLUB_DEBUG
	atomic_long_t nr_slabs;
	atomic_long_t total_objects;
	struct list_head full;
#endif
#endif
};
static inline struct kmem_cache_node *get_node(struct kmem_cache *s, int node)
{
	return s->node[node];
}
#define for_each_kmem_cache_node(__s, __node, __n) \
	for (__node = 0; __node < nr_node_ids; __node++) \
		 if ((__n = get_node(__s, __node)))
#if defined(CONFIG_SLAB) || defined(CONFIG_SLUB_DEBUG)
void dump_unreclaimable_slab(void);
#else
static inline void dump_unreclaimable_slab(void)
{
}
#endif
void ___cache_free(struct kmem_cache *cache, void *x, unsigned long addr);
#ifdef CONFIG_SLAB_FREELIST_RANDOM
int cache_random_seq_create(struct kmem_cache *cachep, unsigned int count,
			gfp_t gfp);
void cache_random_seq_destroy(struct kmem_cache *cachep);
#else
static inline int cache_random_seq_create(struct kmem_cache *cachep,
					unsigned int count, gfp_t gfp)
{
	return 0;
}
static inline void cache_random_seq_destroy(struct kmem_cache *cachep) { }
#endif  
static inline bool slab_want_init_on_alloc(gfp_t flags, struct kmem_cache *c)
{
	if (static_branch_maybe(CONFIG_INIT_ON_ALLOC_DEFAULT_ON,
				&init_on_alloc)) {
		if (c->ctor)
			return false;
		if (c->flags & (SLAB_TYPESAFE_BY_RCU | SLAB_POISON))
			return flags & __GFP_ZERO;
		return true;
	}
	return flags & __GFP_ZERO;
}
static inline bool slab_want_init_on_free(struct kmem_cache *c)
{
	if (static_branch_maybe(CONFIG_INIT_ON_FREE_DEFAULT_ON,
				&init_on_free))
		return !(c->ctor ||
			 (c->flags & (SLAB_TYPESAFE_BY_RCU | SLAB_POISON)));
	return false;
}
#if defined(CONFIG_DEBUG_FS) && defined(CONFIG_SLUB_DEBUG)
void debugfs_slab_release(struct kmem_cache *);
#else
static inline void debugfs_slab_release(struct kmem_cache *s) { }
#endif
#ifdef CONFIG_PRINTK
#define KS_ADDRS_COUNT 16
struct kmem_obj_info {
	void *kp_ptr;
	struct slab *kp_slab;
	void *kp_objp;
	unsigned long kp_data_offset;
	struct kmem_cache *kp_slab_cache;
	void *kp_ret;
	void *kp_stack[KS_ADDRS_COUNT];
	void *kp_free_stack[KS_ADDRS_COUNT];
};
void __kmem_obj_info(struct kmem_obj_info *kpp, void *object, struct slab *slab);
#endif
void __check_heap_object(const void *ptr, unsigned long n,
			 const struct slab *slab, bool to_user);
#ifdef CONFIG_SLUB_DEBUG
void skip_orig_size_check(struct kmem_cache *s, const void *object);
#endif
#endif  
