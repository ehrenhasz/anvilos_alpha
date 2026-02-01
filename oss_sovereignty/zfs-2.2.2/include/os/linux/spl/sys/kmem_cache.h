 

#ifndef _SPL_KMEM_CACHE_H
#define	_SPL_KMEM_CACHE_H

#include <sys/taskq.h>

 
typedef enum kmc_bit {
	KMC_BIT_NODEBUG		= 1,	 
	KMC_BIT_KVMEM		= 7,	 
	KMC_BIT_SLAB		= 8,	 
	KMC_BIT_DEADLOCKED	= 14,	 
	KMC_BIT_GROWING		= 15,	 
	KMC_BIT_REAPING		= 16,	 
	KMC_BIT_DESTROY		= 17,	 
	KMC_BIT_TOTAL		= 18,	 
	KMC_BIT_ALLOC		= 19,	 
	KMC_BIT_MAX		= 20,	 
} kmc_bit_t;

 
typedef enum kmem_cbrc {
	KMEM_CBRC_YES		= 0,	 
	KMEM_CBRC_NO		= 1,	 
	KMEM_CBRC_LATER		= 2,	 
	KMEM_CBRC_DONT_NEED	= 3,	 
	KMEM_CBRC_DONT_KNOW	= 4,	 
} kmem_cbrc_t;

#define	KMC_NODEBUG		(1 << KMC_BIT_NODEBUG)
#define	KMC_KVMEM		(1 << KMC_BIT_KVMEM)
#define	KMC_SLAB		(1 << KMC_BIT_SLAB)
#define	KMC_DEADLOCKED		(1 << KMC_BIT_DEADLOCKED)
#define	KMC_GROWING		(1 << KMC_BIT_GROWING)
#define	KMC_REAPING		(1 << KMC_BIT_REAPING)
#define	KMC_DESTROY		(1 << KMC_BIT_DESTROY)
#define	KMC_TOTAL		(1 << KMC_BIT_TOTAL)
#define	KMC_ALLOC		(1 << KMC_BIT_ALLOC)
#define	KMC_MAX			(1 << KMC_BIT_MAX)

#define	KMC_REAP_CHUNK		INT_MAX
#define	KMC_DEFAULT_SEEKS	1

#define	KMC_RECLAIM_ONCE	0x1	 

extern struct list_head spl_kmem_cache_list;
extern struct rw_semaphore spl_kmem_cache_sem;

#define	SKM_MAGIC			0x2e2e2e2e
#define	SKO_MAGIC			0x20202020
#define	SKS_MAGIC			0x22222222
#define	SKC_MAGIC			0x2c2c2c2c

#define	SPL_KMEM_CACHE_OBJ_PER_SLAB	8	 
#define	SPL_KMEM_CACHE_ALIGN		8	 
#ifdef _LP64
#define	SPL_KMEM_CACHE_MAX_SIZE		32	 
#else
#define	SPL_KMEM_CACHE_MAX_SIZE		4	 
#endif

#define	SPL_MAX_ORDER			(MAX_ORDER - 3)
#define	SPL_MAX_ORDER_NR_PAGES		(1 << (SPL_MAX_ORDER - 1))

#ifdef CONFIG_SLUB
#define	SPL_MAX_KMEM_CACHE_ORDER	PAGE_ALLOC_COSTLY_ORDER
#define	SPL_MAX_KMEM_ORDER_NR_PAGES	(1 << (SPL_MAX_KMEM_CACHE_ORDER - 1))
#else
#define	SPL_MAX_KMEM_ORDER_NR_PAGES	(KMALLOC_MAX_SIZE >> PAGE_SHIFT)
#endif

typedef int (*spl_kmem_ctor_t)(void *, void *, int);
typedef void (*spl_kmem_dtor_t)(void *, void *);

typedef struct spl_kmem_magazine {
	uint32_t		skm_magic;	 
	uint32_t		skm_avail;	 
	uint32_t		skm_size;	 
	uint32_t		skm_refill;	 
	struct spl_kmem_cache	*skm_cache;	 
	unsigned int		skm_cpu;	 
	void			*skm_objs[];	 
} spl_kmem_magazine_t;

typedef struct spl_kmem_obj {
	uint32_t		sko_magic;	 
	void			*sko_addr;	 
	struct spl_kmem_slab	*sko_slab;	 
	struct list_head	sko_list;	 
} spl_kmem_obj_t;

typedef struct spl_kmem_slab {
	uint32_t		sks_magic;	 
	uint32_t		sks_objs;	 
	struct spl_kmem_cache	*sks_cache;	 
	struct list_head	sks_list;	 
	struct list_head	sks_free_list;	 
	unsigned long		sks_age;	 
	uint32_t		sks_ref;	 
} spl_kmem_slab_t;

typedef struct spl_kmem_alloc {
	struct spl_kmem_cache	*ska_cache;	 
	int			ska_flags;	 
	taskq_ent_t		ska_tqe;	 
} spl_kmem_alloc_t;

typedef struct spl_kmem_emergency {
	struct rb_node		ske_node;	 
	unsigned long		ske_obj;	 
} spl_kmem_emergency_t;

typedef struct spl_kmem_cache {
	uint32_t		skc_magic;	 
	uint32_t		skc_name_size;	 
	char			*skc_name;	 
	spl_kmem_magazine_t	**skc_mag;	 
	uint32_t		skc_mag_size;	 
	uint32_t		skc_mag_refill;	 
	spl_kmem_ctor_t		skc_ctor;	 
	spl_kmem_dtor_t		skc_dtor;	 
	void			*skc_private;	 
	void			*skc_vmp;	 
	struct kmem_cache	*skc_linux_cache;  
	unsigned long		skc_flags;	 
	uint32_t		skc_obj_size;	 
	uint32_t		skc_obj_align;	 
	uint32_t		skc_slab_objs;	 
	uint32_t		skc_slab_size;	 
	atomic_t		skc_ref;	 
	taskqid_t		skc_taskqid;	 
	struct list_head	skc_list;	 
	struct list_head	skc_complete_list;  
	struct list_head	skc_partial_list;   
	struct rb_root		skc_emergency_tree;  
	spinlock_t		skc_lock;	 
	spl_wait_queue_head_t	skc_waitq;	 
	uint64_t		skc_slab_fail;	 
	uint64_t		skc_slab_create;   
	uint64_t		skc_slab_destroy;  
	uint64_t		skc_slab_total;	 
	uint64_t		skc_slab_alloc;	 
	uint64_t		skc_slab_max;	 
	uint64_t		skc_obj_total;	 
	uint64_t		skc_obj_alloc;	 
	struct percpu_counter	skc_linux_alloc;    
	uint64_t		skc_obj_max;	 
	uint64_t		skc_obj_deadlock;   
	uint64_t		skc_obj_emergency;  
	uint64_t		skc_obj_emergency_max;  
} spl_kmem_cache_t;
#define	kmem_cache_t		spl_kmem_cache_t

extern spl_kmem_cache_t *spl_kmem_cache_create(const char *name, size_t size,
    size_t align, spl_kmem_ctor_t ctor, spl_kmem_dtor_t dtor,
    void *reclaim, void *priv, void *vmp, int flags);
extern void spl_kmem_cache_set_move(spl_kmem_cache_t *,
    kmem_cbrc_t (*)(void *, void *, size_t, void *));
extern void spl_kmem_cache_destroy(spl_kmem_cache_t *skc);
extern void *spl_kmem_cache_alloc(spl_kmem_cache_t *skc, int flags);
extern void spl_kmem_cache_free(spl_kmem_cache_t *skc, void *obj);
extern void spl_kmem_cache_set_allocflags(spl_kmem_cache_t *skc, gfp_t flags);
extern void spl_kmem_cache_reap_now(spl_kmem_cache_t *skc);
extern void spl_kmem_reap(void);
extern uint64_t spl_kmem_cache_inuse(kmem_cache_t *cache);
extern uint64_t spl_kmem_cache_entry_size(kmem_cache_t *cache);

#define	kmem_cache_create(name, size, align, ctor, dtor, rclm, priv, vmp, fl) \
    spl_kmem_cache_create(name, size, align, ctor, dtor, rclm, priv, vmp, fl)
#define	kmem_cache_set_move(skc, move)	spl_kmem_cache_set_move(skc, move)
#define	kmem_cache_destroy(skc)		spl_kmem_cache_destroy(skc)
 
#ifdef kmem_cache_alloc
#undef kmem_cache_alloc
#endif
#define	kmem_cache_alloc(skc, flags)	spl_kmem_cache_alloc(skc, flags)
#define	kmem_cache_free(skc, obj)	spl_kmem_cache_free(skc, obj)
#define	kmem_cache_reap_now(skc)	spl_kmem_cache_reap_now(skc)
#define	kmem_reap()			spl_kmem_reap()

 
extern int spl_kmem_cache_init(void);
extern void spl_kmem_cache_fini(void);

#endif	 
